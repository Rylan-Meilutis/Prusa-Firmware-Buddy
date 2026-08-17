#include <Marlin/src/core/serial.h>
#include <Marlin/src/gcode/queue.h>
#include <USBSerial.h>
#include <tusb.h>

#include <common/directory.hpp>
#include <common/filename_type.hpp>
#include <common/path_utils.h>
#include <common/crash_dump/dump.hpp>
#include <common/data_exchange.hpp>
#include <marlin_server.hpp>
#include <serial_remote_control.hpp>
#include <rme_protocol_parser.hpp>
#include <rme_file_transfer.hpp>
#include <rme_firmware_status.hpp>
#include <transfers/monitor.hpp>
#include <crc32.h>

#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <timing.h>
#include <buddy/filename_defs.hpp>

extern "C" bool buddy_rme_service_frame(const char *raw_command);

namespace {
constexpr size_t rme_path_buffer_size = filename_defs::path_buffer_size;
constexpr size_t transfer_chunk_size = 48;
constexpr size_t bulk_chunk_size = rme_file_transfer::bulk_payload_size;
constexpr uint8_t bulk_window_size = rme_file_transfer::bulk_window_size;
// USB CDC is a byte stream using 64-byte full-speed endpoint packets. Three
// 512-byte application frames keep the endpoint busy while the complete wire
// window still fits in the target's 2 KiB receive FIFO.
constexpr size_t binary_chunk_size = rme_file_transfer::binary_payload_size;
constexpr uint8_t binary_window_size = rme_file_transfer::binary_window_size;
constexpr uint32_t maximum_file_size = 1024U * 1024U * 1024U;
constexpr uint32_t upload_inactivity_timeout_ms = 10'000;
constexpr uint32_t upload_state_guard = UINT32_C(0x524d4555); // "RMEU"
constexpr const char *firmware_candidate_path = "/usb/FWUPD.RME";
constexpr const char *firmware_marker_path = "/usb/FWUPD.UI";
constexpr const char *firmware_verified_path = "/usb/FWUPD.RME.rme-verified";
constexpr const char *firmware_verified_temp_path = "/usb/FWUPD.RME.rme-verified-tmp";

// A completed bulk command is removed from the CDC FIFO before its storage
// write begins, so the FIFO must retain the other commands in the advertised
// window. A smaller FIFO silently dropped prefixes and joined Base64 tails,
// causing payload text to escape into the ordinary G-code parser.
static_assert(CFG_TUD_CDC_RX_BUFSIZE >= rme_file_transfer::bulk_receive_backlog,
    "CDC RX FIFO is too small for the advertised RME text-bulk window");
static_assert(CFG_TUD_CDC_RX_BUFSIZE >= rme_file_transfer::binary_receive_backlog,
    "CDC RX FIFO is too small for the advertised RME binary window");

struct UploadState {
    uint32_t guard_head = upload_state_guard;
    FILE *file = nullptr;
    uint32_t expected_size = 0;
    uint32_t received = 0;
    std::array<uint8_t, 32> expected_sha {};
    mbedtls_sha256_context sha {};
    bool sha_initialized = false;
    std::array<char, rme_path_buffer_size> final_path {};
    std::array<char, rme_path_buffer_size> partial_path {};
    std::array<char, rme_path_buffer_size> metadata_path {};
    bool bulk = false;
    bool binary = false;
    bool suspended = false;
    bool cdc_connected_at_begin = false;
    uint8_t unacknowledged = 0;
    uint32_t last_activity_ms = 0;
    uint32_t guard_tail = upload_state_guard;
} upload;

std::optional<transfers::Monitor::Slot> upload_slot;

struct BinaryReceiver {
    std::array<uint8_t, rme_file_transfer::binary_header_size> header {};
    uint16_t header_received = 0;
    uint16_t payload_received = 0;
    uint16_t payload_size = 0;
    uint32_t offset = 0;
    uint32_t expected_crc = 0;
    uint8_t unacknowledged = 0;
    uint8_t ascii_abort_matched = 0;
    bool recovering = false;
} binary_receiver;

// Upload and download operations are serialized by the Marlin task. Reuse one
// static payload buffer for both directions instead of reserving two chunks.
// It must not be local: with LTO the largest local is reserved for every file
// service action, which previously overflowed the Marlin task during connect.
std::array<uint8_t, binary_chunk_size> binary_payload {};

// newlib normally allocates a FILE buffer lazily on the first fwrite. Uploads
// commonly start when the GUI/network stacks have already fragmented the
// heap, so that hidden allocation can turn an otherwise streaming transfer
// into an out-of-memory fatal error. Keep exactly one FAT sector in static
// storage and reuse it for the lifetime of the upload instead.
std::array<char, 512> upload_file_buffer {};

constexpr uint32_t upload_metadata_magic = UINT32_C(0x524D4555); // "RMEU"
constexpr uint16_t upload_metadata_version = 1;
struct PersistentUploadMetadata {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t expected_size;
    std::array<uint8_t, 32> expected_sha;
    std::array<char, rme_path_buffer_size> final_path;
};

PersistentUploadMetadata metadata_scratch {};
std::array<char, rme_path_buffer_size> resume_partial_path {};
std::array<char, rme_path_buffer_size> resume_metadata_path {};
std::array<char, rme_path_buffer_size> metadata_temp_path {};
rme_firmware_status::VerifiedMetadata verified_firmware_cache {};
bool verified_firmware_cache_loaded = false;

void write_le16(uint8_t *p, const uint16_t value) {
    p[0] = value;
    p[1] = value >> 8;
}
void write_le32(uint8_t *p, const uint32_t value) {
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

void report_error(const char *code) {
    SERIAL_ECHOPGM("echo:RME_ERROR workflow=file code=");
    SERIAL_ECHOLN(code);
}

void report_upload_error(const char *code, const bool resumable) {
    SERIAL_ECHOPGM("echo:RME_ERROR workflow=file code=");
    SERIAL_ECHO(code);
    SERIAL_ECHOPGM(" offset=");
    SERIAL_ECHO(upload.received);
    SERIAL_ECHOPGM(" resumable=");
    SERIAL_ECHOLN(resumable ? 1 : 0);
}

std::optional<std::string_view> value(const std::string_view command, const std::string_view key) {
    return rme_protocol::value(command, key);
}

std::optional<uint32_t> number(const std::string_view command, const std::string_view key) {
    return rme_protocol::unsigned_number(command, key);
}

bool decode_path(const std::string_view encoded, std::array<char, rme_path_buffer_size> &path) {
    const auto decoded = rme_protocol::usb_path<rme_path_buffer_size>(encoded);
    if (!decoded) {
        return false;
    }
    path = *decoded;
    return true;
}

std::optional<std::array<char, rme_path_buffer_size>> path_value(const std::string_view command, const std::string_view key = "path") {
    const auto encoded = value(command, key);
    std::array<char, rme_path_buffer_size> path {};
    if (!encoded || !decode_path(*encoded, path)) {
        return std::nullopt;
    }
    // FWUPD.BBF is the legacy host-visible staging name. Never leave a
    // completed, discoverable BBF under that name: the bootloader may select
    // it on an unrelated reboot before the user or host explicitly requests
    // a flash. Keep wire compatibility while storing it under a neutral
    // extension that is only opened through the retained M997 request.
    if (strcasecmp(path.data(), "/usb/FWUPD.BBF") == 0) {
        strlcpy(path.data(), "/usb/FWUPD.RME", path.size());
    }
    return path;
}

bool parse_sha256(const std::string_view text, std::array<uint8_t, 32> &result) {
    return rme_protocol::parse_sha256(text, result);
}

bool firmware_armed() {
    struct stat marker {};
    return data_exchange::is_reflash_bbf_sfn("FWUPD.RME")
        && stat(firmware_marker_path, &marker) == 0 && S_ISREG(marker.st_mode);
}

struct FirmwareValidationState {
    FILE *file = nullptr;
    mbedtls_sha256_context sha {};
    uint32_t size = 0;
    uint32_t processed = 0;
    uint32_t started = 0;
    uint32_t last_progress = 0;
    bool sha_initialized = false;
} firmware_validation;

void stop_firmware_validation() {
    if (firmware_validation.file) {
        fclose(firmware_validation.file);
    }
    if (firmware_validation.sha_initialized) {
        mbedtls_sha256_free(&firmware_validation.sha);
    }
    firmware_validation = {};
}

void report_firmware_validating(const bool armed) {
    SERIAL_ECHOPGM("RME_FIRMWARE candidate=1 armed=");
    SERIAL_ECHO(armed ? 1 : 0);
    SERIAL_ECHOPGM(" state=validating path=FWUPD.RME size=");
    SERIAL_ECHO(firmware_validation.size);
    SERIAL_ECHOPGM(" progress=");
    SERIAL_ECHOLN(firmware_validation.processed);
}

bool read_verified_firmware_metadata(const uint32_t candidate_size,
    rme_firmware_status::VerifiedMetadata &metadata) {
    if (verified_firmware_cache_loaded
        && rme_firmware_status::valid(verified_firmware_cache, candidate_size)) {
        metadata = verified_firmware_cache;
        return true;
    }
    FILE *file = fopen(firmware_verified_path, "rb");
    if (!file) {
        return false;
    }
    setvbuf(file, nullptr, _IONBF, 0);
    metadata = {};
    const bool read = fread(&metadata, 1, sizeof(metadata), file) == sizeof(metadata);
    fclose(file);
    if (!read || !rme_firmware_status::valid(metadata, candidate_size)) {
        return false;
    }
    verified_firmware_cache = metadata;
    verified_firmware_cache_loaded = true;
    return true;
}

bool write_verified_firmware_metadata(const uint32_t size, const std::array<uint8_t, 32> &sha256) {
    rme_firmware_status::VerifiedMetadata metadata {};
    metadata.size = size;
    metadata.sha256 = sha256;
    remove(firmware_verified_temp_path);
    FILE *file = fopen(firmware_verified_temp_path, "wb");
    if (!file) {
        return false;
    }
    setvbuf(file, nullptr, _IONBF, 0);
    bool ok = fwrite(&metadata, 1, sizeof(metadata), file) == sizeof(metadata)
        && fflush(file) == 0 && fsync(fileno(file)) == 0;
    ok = fclose(file) == 0 && ok;
    if (ok) {
        remove(firmware_verified_path);
        ok = rename(firmware_verified_temp_path, firmware_verified_path) == 0;
    }
    if (!ok) {
        remove(firmware_verified_temp_path);
        return false;
    }
    verified_firmware_cache = metadata;
    verified_firmware_cache_loaded = true;
    return true;
}

void cache_completed_firmware_upload() {
    if (strcasecmp(upload.final_path.data(), firmware_candidate_path) == 0) {
        // The candidate has already been SHA-verified and atomically
        // published. Preserve that authoritative digest so status queries do
        // not synchronously scan a multi-megabyte BBF.
        write_verified_firmware_metadata(upload.expected_size, upload.expected_sha);
    }
}

bool report_firmware_status() {
    const bool armed = firmware_armed();
    std::array<uint8_t, 32> digest {};
    uint32_t size = 0;
    struct stat candidate_stat {};
    const bool candidate = !upload.file && stat(firmware_candidate_path, &candidate_stat) == 0
        && S_ISREG(candidate_stat.st_mode) && candidate_stat.st_size > 0
        && static_cast<uint64_t>(candidate_stat.st_size) <= maximum_file_size;
    if (candidate) {
        size = static_cast<uint32_t>(candidate_stat.st_size);
        rme_firmware_status::VerifiedMetadata metadata {};
        if (read_verified_firmware_metadata(size, metadata)) {
            digest = metadata.sha256;
        } else if (firmware_validation.file) {
            report_firmware_validating(armed);
            return true;
        } else {
            firmware_validation.file = fopen(firmware_candidate_path, "rb");
            if (!firmware_validation.file) {
                SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=query_read_failed");
                return false;
            }
            setvbuf(firmware_validation.file, nullptr, _IONBF, 0);
            firmware_validation.size = size;
            firmware_validation.started = ticks_ms();
            firmware_validation.last_progress = firmware_validation.started;
            mbedtls_sha256_init(&firmware_validation.sha);
            firmware_validation.sha_initialized = true;
            if (mbedtls_sha256_starts_ret(&firmware_validation.sha, false) != 0) {
                stop_firmware_validation();
                SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=query_hash_failed");
                return false;
            }
            report_firmware_validating(armed);
            return true;
        }
    } else if (candidate_stat.st_mode != 0) {
        SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=invalid_candidate");
        return false;
    }
    SERIAL_ECHOPGM("RME_FIRMWARE candidate=");
    SERIAL_ECHO(candidate ? 1 : 0);
    SERIAL_ECHOPGM(" armed=");
    SERIAL_ECHO(armed ? 1 : 0);
    SERIAL_ECHOPGM(" state=");
    SERIAL_ECHO(armed ? "restarting" : candidate ? "ready"
                                                 : "idle");
    if (candidate) {
        SERIAL_ECHOPGM(" path=FWUPD.RME size=");
        SERIAL_ECHO(size);
        SERIAL_ECHOPGM(" sha256=");
        constexpr char hex[] = "0123456789abcdef";
        for (const uint8_t byte : digest) {
            SERIAL_CHAR(hex[byte >> 4]);
            SERIAL_CHAR(hex[byte & 0x0f]);
        }
    }
    SERIAL_EOL();
    return true;
}

void release_upload_slot(const transfers::Monitor::Outcome outcome) {
    if (upload_slot) {
        upload_slot->done(outcome);
        upload_slot.reset();
    }
}

bool upload_state_valid() {
    return upload.guard_head == upload_state_guard && upload.guard_tail == upload_state_guard;
}

// Do not dereference or free anything from a state block whose canary failed:
// the FILE/SHA members are no longer trustworthy.  Returning to line mode and
// releasing the shared latch is safer than turning detected corruption into a
// second fault inside fclose/fwrite.  The durable metadata remains available
// for a clean reboot/resume and the diagnostic identifies this exact path.
void abandon_corrupt_upload_state() {
    upload = {};
    binary_receiver = {};
    release_upload_slot(transfers::Monitor::Outcome::ErrorOther);
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
    report_error("state_corrupt");
}

bool claim_upload_slot(const char *path, const uint32_t expected_size) {
    auto slot = transfers::Monitor::instance.allocate(transfers::Monitor::Type::Link, path, expected_size);
    if (!slot) {
        return false;
    }
    upload_slot.emplace(std::move(*slot));
    return true;
}

void reset_upload(const bool remove_partial,
    const transfers::Monitor::Outcome outcome = transfers::Monitor::Outcome::ErrorOther,
    const bool release_slot = true) {
    if (upload.file) {
        fclose(upload.file);
    }
    if (upload.sha_initialized) {
        mbedtls_sha256_free(&upload.sha);
    }
    if (remove_partial && upload.partial_path[0]) {
        remove(upload.partial_path.data());
    }
    if (upload.metadata_path[0]) {
        remove(upload.metadata_path.data());
    }
    if (release_slot) {
        release_upload_slot(outcome);
    }
    upload = {};
    binary_receiver = {};
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
}

void forget_upload_state(const bool release_slot = true) {
    if (upload.file) {
        fclose(upload.file);
    }
    if (upload.sha_initialized) {
        mbedtls_sha256_free(&upload.sha);
    }
    if (release_slot) {
        release_upload_slot(transfers::Monitor::Outcome::Stopped);
    }
    upload = {};
    binary_receiver = {};
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
}

bool persist_upload_metadata() {
    if (!upload.metadata_path[0]) {
        return false;
    }
    metadata_scratch = PersistentUploadMetadata {
        .magic = upload_metadata_magic,
        .version = upload_metadata_version,
        .reserved = 0,
        .expected_size = upload.expected_size,
        .expected_sha = upload.expected_sha,
        .final_path = upload.final_path,
    };
    if (snprintf(metadata_temp_path.data(), metadata_temp_path.size(), "%s.rme-tmp", upload.final_path.data())
        >= static_cast<int>(metadata_temp_path.size())) {
        return false;
    }
    remove(metadata_temp_path.data());
    FILE *file = fopen(metadata_temp_path.data(), "wb");
    if (!file) {
        return false;
    }
    setvbuf(file, nullptr, _IONBF, 0);
    bool ok = fwrite(&metadata_scratch, 1, sizeof(metadata_scratch), file) == sizeof(metadata_scratch)
        && fflush(file) == 0 && fsync(fileno(file)) == 0;
    ok = fclose(file) == 0 && ok;
    if (ok) {
        remove(upload.metadata_path.data());
        ok = rename(metadata_temp_path.data(), upload.metadata_path.data()) == 0;
    }
    if (!ok) {
        remove(metadata_temp_path.data());
    }
    return ok;
}

bool load_persistent_upload(const std::array<char, rme_path_buffer_size> &final_path,
    const uint32_t expected_size, const std::array<uint8_t, 32> &expected_sha) {
    if (snprintf(resume_partial_path.data(), resume_partial_path.size(), "%s.rme-part", final_path.data()) >= static_cast<int>(resume_partial_path.size())
        || snprintf(resume_metadata_path.data(), resume_metadata_path.size(), "%s.rme-meta", final_path.data()) >= static_cast<int>(resume_metadata_path.size())
        || snprintf(metadata_temp_path.data(), metadata_temp_path.size(), "%s.rme-tmp", final_path.data()) >= static_cast<int>(metadata_temp_path.size())) {
        return false;
    }

    const auto read_metadata = [](const char *path) {
        metadata_scratch = {};
        FILE *file = fopen(path, "rb");
        if (!file) {
            return false;
        }
        setvbuf(file, nullptr, _IONBF, 0);
        const bool read = fread(&metadata_scratch, 1, sizeof(metadata_scratch), file) == sizeof(metadata_scratch);
        fclose(file);
        return read;
    };
    const auto metadata_matches = [&] {
        return metadata_scratch.magic == upload_metadata_magic
            && metadata_scratch.version == upload_metadata_version
            && metadata_scratch.expected_size == expected_size
            && metadata_scratch.expected_sha == expected_sha
            && metadata_scratch.final_path == final_path;
    };
    bool recovered_temp = false;
    if (!read_metadata(resume_metadata_path.data()) || !metadata_matches()) {
        if (!read_metadata(metadata_temp_path.data()) || !metadata_matches()) {
            return false;
        }
        recovered_temp = true;
        remove(resume_metadata_path.data());
        if (rename(metadata_temp_path.data(), resume_metadata_path.data()) == 0) {
            recovered_temp = false;
        }
    }

    upload = {};
    upload.expected_size = expected_size;
    upload.expected_sha = expected_sha;
    upload.final_path = final_path;
    upload.partial_path = resume_partial_path;
    upload.metadata_path = recovered_temp ? metadata_temp_path : resume_metadata_path;
    upload.suspended = true;
    return true;
}

// Leave a failed transfer in line mode while retaining its committed prefix.
// A matching BEGIN can reopen and re-hash it, including when changing from raw
// binary to the Base64 fallback. This also guarantees that a media failure can
// never leave the serial receiver permanently owned by raw mode.
void suspend_upload(const transfers::Monitor::Outcome outcome = transfers::Monitor::Outcome::Stopped) {
    if (upload.file) {
        fflush(upload.file);
        fclose(upload.file);
        upload.file = nullptr;
    }
    struct stat partial {};
    if (upload.partial_path[0] && stat(upload.partial_path.data(), &partial) == 0
        && S_ISREG(partial.st_mode) && partial.st_size >= 0
        && static_cast<uint64_t>(partial.st_size) <= upload.expected_size) {
        upload.received = static_cast<uint32_t>(partial.st_size);
    }
    if (upload.sha_initialized) {
        mbedtls_sha256_free(&upload.sha);
        upload.sha_initialized = false;
    }
    upload.binary = false;
    upload.bulk = false;
    upload.suspended = true;
    release_upload_slot(outcome);
    binary_receiver = {};
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
}

bool resume_suspended_upload(const bool bulk, const bool binary) {
    struct stat partial {};
    if (!upload.suspended || stat(upload.partial_path.data(), &partial) != 0
        || !S_ISREG(partial.st_mode) || partial.st_size < 0
        || static_cast<uint64_t>(partial.st_size) > upload.expected_size
        || !(upload.file = fopen(upload.partial_path.data(), "rb+"))
        || setvbuf(upload.file, upload_file_buffer.data(), _IOFBF, upload_file_buffer.size()) != 0) {
        return false;
    }

    mbedtls_sha256_init(&upload.sha);
    upload.sha_initialized = true;
    if (mbedtls_sha256_starts_ret(&upload.sha, false)) {
        return false;
    }

    upload.received = 0;
    while (upload.received < static_cast<uint32_t>(partial.st_size)) {
        const size_t wanted = std::min<size_t>(binary_payload.size(), static_cast<size_t>(partial.st_size) - upload.received);
        const size_t count = fread(binary_payload.data(), 1, wanted, upload.file);
        if (count != wanted || mbedtls_sha256_update_ret(&upload.sha, binary_payload.data(), count)) {
            return false;
        }
        upload.received += count;
    }
    if (fseek(upload.file, 0, SEEK_END)) {
        return false;
    }
    upload.bulk = bulk;
    upload.binary = binary;
    upload.suspended = false;
    upload.cdc_connected_at_begin = tud_cdc_connected();
    upload.unacknowledged = 0;
    upload.last_activity_ms = ticks_ms();
    binary_receiver = {};
    if (upload_slot && upload.received) {
        upload_slot->progress(upload.received);
    }
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, upload.received, upload.expected_size);
    return true;
}

bool root_allowed(const std::array<char, rme_path_buffer_size> &path, const std::string_view action) {
    return path[5] != '\0' || rme_protocol::action_is(action, "LIST") || rme_protocol::action_is(action, "STAT");
}

bool is_firmware_candidate(const std::array<char, rme_path_buffer_size> &path) {
    return strcasecmp(path.data(), firmware_candidate_path) == 0;
}

// FatFS does not consistently provide POSIX rename-over-existing semantics.
// Preserve the previous file until the verified partial upload has taken its
// place so every upload mode can safely replace an existing destination.
bool replace_uploaded_file(const char *partial_path, const char *final_path) {
    std::array<char, rme_path_buffer_size> backup_path {};
    if (snprintf(backup_path.data(), backup_path.size(), "%s.rme-old", final_path) >= static_cast<int>(backup_path.size())) {
        return false;
    }

    struct stat destination {};
    if (stat(final_path, &destination) != 0) {
        const bool installed = rename(partial_path, final_path) == 0;
        if (installed) {
            // Also completes cleanup after a reset interrupted a previous
            // replacement between moving the old and installing the new file.
            remove(backup_path.data());
        }
        return installed;
    }
    if (!S_ISREG(destination.st_mode)) {
        return false;
    }

    remove(backup_path.data());
    if (rename(final_path, backup_path.data()) != 0) {
        return false;
    }
    if (rename(partial_path, final_path) == 0) {
        remove(backup_path.data());
        return true;
    }

    // Best-effort rollback leaves the original name and contents intact.
    rename(backup_path.data(), final_path);
    return false;
}

bool finish_current_upload(const char *complete_prefix) {
    if (!upload.file || upload.received != upload.expected_size) {
        report_error("size_mismatch");
        return false;
    }
    std::array<uint8_t, 32> actual {};
    bool ok = fflush(upload.file) == 0 && fsync(fileno(upload.file)) == 0
        && mbedtls_sha256_finish_ret(&upload.sha, actual.data()) == 0 && actual == upload.expected_sha;
    fclose(upload.file);
    upload.file = nullptr;
    mbedtls_sha256_free(&upload.sha);
    upload.sha_initialized = false;
    if (ok) {
        ok = replace_uploaded_file(upload.partial_path.data(), upload.final_path.data());
    }
    if (!ok) {
        reset_upload(true);
        report_error("finalize_failed");
        return false;
    }
    cache_completed_firmware_upload();
    SERIAL_ECHOPGM(complete_prefix);
    SERIAL_ECHOLN(upload.final_path.data() + 5);
    reset_upload(false, transfers::Monitor::Outcome::Finished);
    return true;
}
} // namespace

extern "C" bool buddy_rme_binary_upload_active() {
    if (!upload_state_valid()) {
        abandon_corrupt_upload_state();
        return false;
    }
    if (upload.file
        && (!tud_mounted() || (upload.cdc_connected_at_begin && !tud_cdc_connected()))) {
        const bool binary = upload.binary;
        suspend_upload();
        SERIAL_ECHOPGM(binary ? "RME_FILE_BINARY_SUSPENDED offset=" : "RME_FILE_SUSPENDED offset=");
        SERIAL_ECHO(upload.received);
        SERIAL_ECHOLNPGM(" resumable=1 reason=disconnect");
    } else if (upload.file && upload.last_activity_ms
        && ticks_diff(ticks_ms(), upload.last_activity_ms + upload_inactivity_timeout_ms) >= 0) {
        const bool binary = upload.binary;
        suspend_upload();
        SERIAL_ECHOPGM(binary ? "RME_FILE_BINARY_SUSPENDED offset=" : "RME_FILE_SUSPENDED offset=");
        SERIAL_ECHO(upload.received);
        SERIAL_ECHOLNPGM(" resumable=1 reason=inactivity_timeout");
    }
    return upload.file && upload.binary;
}

// Advance legacy candidate validation in one bounded storage slice. This runs
// once per serial scheduler pass, outside command dispatch, so G-code and RME
// commands remain responsive while the candidate is being hashed.
extern "C" void buddy_rme_firmware_service_tick() {
    if (!firmware_validation.file) {
        return;
    }

    constexpr uint8_t chunks_per_slice = 4;
    bool complete = false;
    bool failed = false;
    for (uint8_t chunk = 0; chunk < chunks_per_slice && !complete; ++chunk) {
        const size_t count = fread(binary_payload.data(), 1, binary_payload.size(), firmware_validation.file);
        if (count && mbedtls_sha256_update_ret(&firmware_validation.sha, binary_payload.data(), count) != 0) {
            failed = true;
            break;
        }
        firmware_validation.processed += count;
        if (count < binary_payload.size()) {
            const bool read_error = ferror(firmware_validation.file);
            complete = rme_firmware_status::complete_read_valid(
                firmware_validation.processed, firmware_validation.size, true, read_error);
            failed = !complete;
        }
        const uint32_t now = ticks_ms();
        if (rme_firmware_status::elapsed(now, firmware_validation.started, rme_firmware_status::query_hash_timeout_ms)) {
            stop_firmware_validation();
            SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=query_timeout");
            return;
        }
    }

    const uint32_t now = ticks_ms();
    if (!complete && !failed
        && rme_firmware_status::elapsed(now, firmware_validation.last_progress, rme_firmware_status::query_busy_interval_ms)) {
        firmware_validation.last_progress = now;
        SERIAL_ECHOLNPGM("echo:busy: processing");
        report_firmware_validating(firmware_armed());
    }
    if (!complete && !failed) {
        return;
    }
    if (failed) {
        stop_firmware_validation();
        SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=query_read_failed");
        return;
    }

    std::array<uint8_t, 32> digest {};
    const uint32_t size = firmware_validation.size;
    if (mbedtls_sha256_finish_ret(&firmware_validation.sha, digest.data()) != 0) {
        stop_firmware_validation();
        SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=query_hash_failed");
        return;
    }
    stop_firmware_validation();
    write_verified_firmware_metadata(size, digest);
    SERIAL_ECHOPGM("RME_FIRMWARE candidate=1 armed=");
    SERIAL_ECHO(firmware_armed() ? 1 : 0);
    SERIAL_ECHOPGM(" state=ready path=FWUPD.RME size=");
    SERIAL_ECHO(size);
    SERIAL_ECHOPGM(" sha256=");
    constexpr char hex[] = "0123456789abcdef";
    for (const uint8_t byte : digest) {
        SERIAL_CHAR(hex[byte >> 4]);
        SERIAL_CHAR(hex[byte & 0x0f]);
    }
    SERIAL_EOL();
}

extern "C" void buddy_rme_binary_upload_byte(const uint8_t byte) {
    if (!upload_state_valid()) {
        abandon_corrupt_upload_state();
        return;
    }
    if (!buddy_rme_binary_upload_active()) {
        return;
    }
    upload.last_activity_ms = ticks_ms();

    // Compatibility escape for hosts which cannot inject the raw 0xffffffff
    // frame after their binary transport has failed. Ordinary line parsing is
    // unavailable while raw mode owns RX, so recognize only this exact command
    // locally and require a line terminator. It is deliberately not a general
    // ASCII parser and has no allocation or G-code queue dependency.
    if (rme_file_transfer::consume_text_abort(byte, binary_receiver.ascii_abort_matched, binary_receiver.recovering)) {
        suspend_upload();
        SERIAL_ECHOPGM("RME_FILE_BINARY_ABORTED offset=");
        SERIAL_ECHO(upload.received);
        SERIAL_ECHOLNPGM(" resumable=1 transport=text");
        return;
    }

    if (binary_receiver.header_received < binary_receiver.header.size()) {
        const auto scan = rme_file_transfer::scan_binary_header_byte(
            binary_receiver.header, binary_receiver.header_received, byte,
            upload.received, upload.expected_size, binary_chunk_size,
            binary_receiver.offset, binary_receiver.payload_size,
            binary_receiver.expected_crc);
        if (scan == rme_file_transfer::HeaderScanResult::incomplete) {
            return;
        }
        if (scan == rme_file_transfer::HeaderScanResult::invalid) {
            if (!binary_receiver.recovering) {
                SERIAL_ECHOPGM("RME_FILE_BINARY_NACK offset=");
                SERIAL_ECHO(upload.received);
                SERIAL_ECHOLNPGM(" reason=header_invalid recovering=1");
            }
            binary_receiver.payload_received = 0;
            binary_receiver.recovering = true;
            return;
        }

        if (binary_receiver.payload_size == 0) {
            if (binary_receiver.offset == rme_file_transfer::abort_frame_offset) {
                suspend_upload();
                SERIAL_ECHOPGM("RME_FILE_BINARY_ABORTED offset=");
                SERIAL_ECHO(upload.received);
                SERIAL_ECHOLNPGM(" resumable=1");
            } else if (binary_receiver.offset == upload.received && upload.received == upload.expected_size) {
                finish_current_upload("RME_FILE_BINARY_COMPLETE path=");
            } else {
                SERIAL_ECHOPGM("RME_FILE_BINARY_NACK offset=");
                SERIAL_ECHO(upload.received);
                SERIAL_ECHOLNPGM(" reason=completion_offset_mismatch");
                const uint8_t unacknowledged = binary_receiver.unacknowledged;
                binary_receiver = {};
                binary_receiver.unacknowledged = unacknowledged;
            }
            return;
        }
        return;
    }

    binary_payload[binary_receiver.payload_received++] = byte;
    if (binary_receiver.payload_received != binary_receiver.payload_size) {
        return;
    }

    if (binary_receiver.offset == rme_file_transfer::control_frame_offset) {
        const bool valid_control = binary_receiver.payload_size < binary_payload.size()
            && crc32_sw(binary_payload.data(), binary_receiver.payload_size, 0) == binary_receiver.expected_crc;
        if (!valid_control) {
            SERIAL_ECHOPGM("RME_FILE_BINARY_CONTROL_NACK reason=");
            SERIAL_ECHOLN(binary_receiver.payload_size >= binary_payload.size() ? "chunk_too_large" : "crc_mismatch");
        } else {
            binary_payload[binary_receiver.payload_size] = '\0';
            const std::string_view control { reinterpret_cast<char *>(binary_payload.data()), binary_receiver.payload_size };
            if (!rme_protocol::is_service_frame(control)
                || control.starts_with("@RME FILE WRITE") || control.starts_with("@RME FILE READ")
                || control.starts_with("@RME FILE DELETE") || control.starts_with("@RME FILE RENAME")
                || control.starts_with("@RME FILE FLASH") || control.starts_with("@RME FILE PRINT")) {
                SERIAL_ECHOLNPGM("RME_FILE_BINARY_CONTROL_NACK reason=unsupported_control");
            } else if (control == "@RME FILE ABORT") {
                suspend_upload();
                SERIAL_ECHOPGM("RME_FILE_BINARY_ABORTED offset=");
                SERIAL_ECHO(upload.received);
                SERIAL_ECHOLNPGM(" resumable=1 transport=control");
                return;
            } else {
                buddy_rme_service_frame(reinterpret_cast<char *>(binary_payload.data()));
                SERIAL_ECHOLNPGM("RME_FILE_BINARY_CONTROL_COMPLETE");
                SERIAL_ECHOLNPGM("ok");
            }
        }
        const uint8_t unacknowledged = binary_receiver.unacknowledged;
        binary_receiver = {};
        binary_receiver.unacknowledged = unacknowledged;
        return;
    }

    const auto frame_error = rme_file_transfer::classify_binary_frame(binary_receiver.offset, upload.received,
        binary_receiver.payload_size, upload.expected_size,
        crc32_sw(binary_payload.data(), binary_receiver.payload_size, 0) == binary_receiver.expected_crc);
    const char *failure = rme_file_transfer::diagnostic_code(frame_error);
    if (failure) {
        if (!binary_receiver.recovering) {
            SERIAL_ECHOPGM("RME_FILE_BINARY_NACK offset=");
            SERIAL_ECHO(upload.received);
            SERIAL_ECHOPGM(" reason=");
            SERIAL_ECHO(failure);
            SERIAL_ECHOLNPGM(" recovering=1");
        }
        const uint8_t ascii_abort_matched = binary_receiver.ascii_abort_matched;
        const uint8_t unacknowledged = binary_receiver.unacknowledged;
        binary_receiver = {};
        binary_receiver.ascii_abort_matched = ascii_abort_matched;
        binary_receiver.unacknowledged = unacknowledged;
        binary_receiver.recovering = true;
        return;
    }
    if (fwrite(binary_payload.data(), 1, binary_receiver.payload_size, upload.file) != binary_receiver.payload_size) {
        suspend_upload(transfers::Monitor::Outcome::ErrorStorage);
        report_upload_error("disk_write_failed", true);
        return;
    }
    if (mbedtls_sha256_update_ret(&upload.sha, binary_payload.data(), binary_receiver.payload_size)) {
        suspend_upload(transfers::Monitor::Outcome::ErrorOther);
        report_upload_error("hash_failed", true);
        return;
    }

    upload.received += binary_receiver.payload_size;
    if (upload_slot) {
        upload_slot->progress(binary_receiver.payload_size);
    }
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, upload.received, upload.expected_size);
    const bool acknowledge = ++binary_receiver.unacknowledged >= binary_window_size || upload.received == upload.expected_size;
    const uint8_t unacknowledged = binary_receiver.unacknowledged;
    const uint8_t ascii_abort_matched = binary_receiver.ascii_abort_matched;
    binary_receiver = {};
    binary_receiver.unacknowledged = acknowledge ? 0 : unacknowledged;
    binary_receiver.ascii_abort_matched = ascii_abort_matched;
    if (acknowledge) {
        SERIAL_ECHOPGM("RME_FILE_BINARY_ACK offset=");
        SERIAL_ECHOLN(upload.received);
    }
}

extern "C" bool buddy_rme_file_service(const char *raw_command) {
    constexpr std::string_view prefix = "@RME FILE ";
    const std::string_view command { raw_command, strcspn(raw_command, "*") };
    if (!command.starts_with(prefix)) {
        return false;
    }
    if (!upload_state_valid()) {
        abandon_corrupt_upload_state();
        return true;
    }
    const auto action = command.substr(prefix.size());
    const auto action_is = [](const std::string_view candidate, const std::string_view expected) {
        return rme_protocol::action_is(candidate, expected);
    };

    if (action_is(action, "CAPS")) {
        SERIAL_ECHOPGM("RME_FILE_CAPS root=/usb chunk=");
        SERIAL_ECHO(transfer_chunk_size);
        SERIAL_ECHOPGM(" bulk=1 bulk_chunk=");
        SERIAL_ECHO(bulk_chunk_size);
        SERIAL_ECHOPGM(" bulk_window=");
        SERIAL_ECHO(bulk_window_size);
        SERIAL_ECHOPGM(" binary=1 binary_chunk=");
        SERIAL_ECHO(binary_chunk_size);
        SERIAL_ECHOPGM(" binary_window=");
        SERIAL_ECHO(binary_window_size);
        SERIAL_ECHOPGM(" binary_control=1 binary_control_offset=");
        SERIAL_ECHO(rme_file_transfer::control_frame_offset);
        SERIAL_ECHOPGM(" binary_timeout_ms=");
        SERIAL_ECHO(upload_inactivity_timeout_ms);
        SERIAL_ECHOPGM(" upload_timeout_ms=");
        SERIAL_ECHO(upload_inactivity_timeout_ms);
        SERIAL_ECHOPGM(" resumable_abort=1 binary_read=1 binary_read_chunk=");
        SERIAL_ECHO(binary_chunk_size);
        SERIAL_ECHOPGM(" max_size=");
        SERIAL_ECHO(maximum_file_size);
        SERIAL_ECHOLNPGM(" list=1 stat=1 read=1 write=1 overwrite=1 delete=1 rename=1 mkdir=1 print=1 flash=1 firmware_status=1 firmware_unstage=1 crash_dump=1 durable_resume=1 shared_transfer_latch=1");
        return true;
    }
    if (action_is(action, "ABORT")) {
        reset_upload(true, transfers::Monitor::Outcome::Stopped);
        SERIAL_ECHOLNPGM("RME_FILE_ABORTED");
        return true;
    }

    const bool state_only = action_is(action, "WRITE_CHUNK") || action_is(action, "WRITE_END")
        || action_is(action, "WRITE_BULK_CHUNK") || action_is(action, "WRITE_BULK_END");
    const auto path = state_only ? std::optional<std::array<char, rme_path_buffer_size>> {} : path_value(command);
    if (!state_only && (!path || !root_allowed(*path, action))) {
        report_error(path ? "root_protected" : "invalid_path");
        return true;
    }

    if (action_is(action, "STAT")) {
        struct stat st {};
        if (stat(path->data(), &st)) {
            report_error("not_found");
        } else {
            SERIAL_ECHOPGM("RME_FILE_STAT path=");
            SERIAL_ECHO(path->data() + 5);
            SERIAL_ECHOPGM(" type=");
            SERIAL_ECHO(S_ISDIR(st.st_mode) ? "dir" : "file");
            SERIAL_ECHOPGM(" size=");
            SERIAL_ECHO(static_cast<uint32_t>(st.st_size));
            SERIAL_ECHOPGM(" mtime=");
            SERIAL_ECHOLN(static_cast<uint32_t>(st.st_mtime));
        }
    } else if (action_is(action, "LIST")) {
        Directory directory(path->data());
        if (!directory) {
            report_error("not_directory");
        } else {
            while (const dirent *entry = directory.read()) {
                if (!entry->d_name[0] || entry->d_name[0] == '.') {
                    continue;
                }
                if (filename_is_rme_private(entry->d_name)) {
                    continue;
                }
                std::array<char, rme_path_buffer_size> child {};
                const size_t path_length = strlen(path->data());
                if (snprintf(child.data(), child.size(), "%s%s%s", path->data(), path_length && (*path)[path_length - 1] == '/' ? "" : "/", entry->d_name) >= static_cast<int>(child.size())) {
                    continue;
                }
                struct stat st {};
                if (stat(child.data(), &st)) {
                    continue;
                }
                SERIAL_ECHOPGM("RME_FILE_ENTRY name=");
                SERIAL_ECHO(entry->d_name);
                SERIAL_ECHOPGM(" type=");
                SERIAL_ECHO(S_ISDIR(st.st_mode) ? "dir" : "file");
                SERIAL_ECHOPGM(" size=");
                SERIAL_ECHOLN(static_cast<uint32_t>(st.st_size));
            }
            SERIAL_ECHOLNPGM("RME_FILE_LIST_END");
        }
    } else if (action_is(action, "READ_BINARY")) {
        const uint32_t offset = number(command, "offset").value_or(0);
        const uint32_t length = number(command, "length").value_or(binary_chunk_size);
        struct stat st {};
        if (transfers::Monitor::instance.id().has_value()) {
            report_error("transfer_busy");
        } else if (!length || length > binary_chunk_size || offset > maximum_file_size) {
            report_error("invalid_range");
        } else if (stat(path->data(), &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || static_cast<uint64_t>(st.st_size) > maximum_file_size) {
            report_error("read_failed");
        } else if (FILE *file = fopen(path->data(), "rb")) {
            // The application buffer already bounds this read. Do not let
            // newlib allocate a second hidden stream buffer from the heap.
            setvbuf(file, nullptr, _IONBF, 0);
            if (fseek(file, offset, SEEK_SET)) {
                fclose(file);
                report_error("read_failed");
                return true;
            }
            const size_t count = fread(binary_payload.data(), 1, length, file);
            const bool read_error = ferror(file);
            fclose(file);
            if (read_error) {
                report_error("read_failed");
                return true;
            }

            std::array<uint8_t, 10> header {};
            write_le32(header.data(), offset);
            write_le16(header.data() + 4, static_cast<uint16_t>(count));
            write_le32(header.data() + 6, crc32_sw(binary_payload.data(), count, 0));
            const uint32_t file_size = static_cast<uint32_t>(st.st_size);
            const uint32_t next = offset + count;
            const bool eof = next >= file_size;
            serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, std::min(next, file_size), file_size);
            SERIAL_ECHOPGM("RME_FILE_BINARY_READ_READY path=");
            SERIAL_ECHO(path->data() + 5);
            SERIAL_ECHOPGM(" offset=");
            SERIAL_ECHO(offset);
            SERIAL_ECHOPGM(" length=");
            SERIAL_ECHO(count);
            SERIAL_ECHOLNPGM(" header=10 endian=little crc=crc32");
            SERIAL_FLUSHTX();
            SerialUSB.cdc_write_sync(header.data(), header.size());
            if (count) {
                SerialUSB.cdc_write_sync(binary_payload.data(), count);
            }
            SERIAL_ECHOPGM("RME_FILE_BINARY_READ_COMPLETE next=");
            SERIAL_ECHO(next);
            SERIAL_ECHOPGM(" eof=");
            SERIAL_ECHOLN(eof ? 1 : 0);
            if (eof) {
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
            }
        } else {
            report_error("read_failed");
        }
    } else if (action_is(action, "READ")) {
        const uint32_t offset = number(command, "offset").value_or(0);
        const uint32_t length = number(command, "length").value_or(transfer_chunk_size);
        if (transfers::Monitor::instance.id().has_value()) {
            report_error("transfer_busy");
        } else if (!length || length > transfer_chunk_size) {
            report_error("invalid_range");
        } else if (FILE *file = fopen(path->data(), "rb")) {
            setvbuf(file, nullptr, _IONBF, 0);
            std::array<uint8_t, transfer_chunk_size> raw {};
            if (fseek(file, offset, SEEK_SET)) {
                fclose(file);
                report_error("read_failed");
                return true;
            }
            const size_t count = fread(raw.data(), 1, length, file);
            const bool eof = feof(file);
            fclose(file);
            std::array<unsigned char, 65> encoded {};
            size_t encoded_size = 0;
            if (mbedtls_base64_encode(encoded.data(), encoded.size(), &encoded_size, raw.data(), count)) {
                report_error("encode_failed");
            } else {
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file);
                SERIAL_ECHOPGM("RME_FILE_DATA path=");
                SERIAL_ECHO(path->data() + 5);
                SERIAL_ECHOPGM(" offset=");
                SERIAL_ECHO(offset);
                SERIAL_ECHOPGM(" length=");
                SERIAL_ECHO(count);
                SERIAL_ECHOPGM(" eof=");
                SERIAL_ECHO(eof ? 1 : 0);
                SERIAL_ECHOPGM(" data=");
                SERIAL_ECHOLN(reinterpret_cast<char *>(encoded.data()));
            }
        } else {
            report_error("read_failed");
        }
    } else if (action_is(action, "WRITE_BEGIN") || action_is(action, "WRITE_BULK_BEGIN") || action_is(action, "WRITE_BINARY_BEGIN")) {
        const bool bulk = action_is(action, "WRITE_BULK_BEGIN");
        const bool binary = action_is(action, "WRITE_BINARY_BEGIN");
        const auto size = number(command, "size");
        const auto sha = value(command, "sha256");
        if (upload.file) {
            report_error("upload_state");
        } else if (!marlin_server::printer_idle()) {
            report_error("printer_busy");
        } else if (!size || !*size || *size > maximum_file_size || !sha) {
            report_error("invalid_upload");
        } else {
            std::array<uint8_t, 32> requested_sha {};
            const bool valid_sha = parse_sha256(*sha, requested_sha);
            if (!valid_sha) {
                report_error("invalid_upload");
                return true;
            }
            // Acquire the shared storage latch before loading durable resume
            // metadata or changing any transport/upload state. A competing
            // Connect or Link transfer therefore gets a side-effect-free
            // transfer_busy rejection.
            if (!claim_upload_slot(path->data(), *size)) {
                report_error("transfer_busy");
                return true;
            }
            if (is_firmware_candidate(*path)) {
                stop_firmware_validation();
            }
            bool matching_resume = upload.suspended && upload.expected_size == *size
                && upload.final_path == *path && upload.expected_sha == requested_sha;
            if (!matching_resume) {
                matching_resume = load_persistent_upload(*path, *size, requested_sha);
            }
            if (matching_resume && resume_suspended_upload(bulk, binary)) {
                if (binary) {
                    SERIAL_ECHOPGM("RME_FILE_BINARY_READY offset=");
                    SERIAL_ECHO(upload.received);
                    SERIAL_ECHOPGM(" chunk=");
                    SERIAL_ECHO(binary_chunk_size);
                    SERIAL_ECHOPGM(" window=");
                    SERIAL_ECHO(binary_window_size);
                    SERIAL_ECHOLNPGM(" header=10 endian=little crc=crc32 resumed=1");
                } else if (bulk) {
                    SERIAL_ECHOPGM("RME_FILE_BULK_READY offset=");
                    SERIAL_ECHO(upload.received);
                    SERIAL_ECHOLNPGM(" chunk=384 window=4 resumed=1");
                } else {
                    SERIAL_ECHOPGM("RME_FILE_WRITE_READY offset=");
                    SERIAL_ECHO(upload.received);
                    SERIAL_ECHOLNPGM(" chunk=48 resumed=1");
                }
                return true;
            }
            if (matching_resume) {
                // A matching durable partial is authoritative. A transient
                // media/open/read/hash failure must not erase it or silently
                // restart at offset zero; leave line mode restored and let a
                // later matching BEGIN retry from the committed file size.
                suspend_upload(transfers::Monitor::Outcome::ErrorStorage);
                report_upload_error("resume_failed", true);
                return true;
            }
            // Select a different durable job without deleting the previously
            // selected job's partial and metadata.
            forget_upload_state(false);
            upload.final_path = *path;
            if (snprintf(upload.partial_path.data(), upload.partial_path.size(), "%s.rme-part", path->data()) >= static_cast<int>(upload.partial_path.size())
                || snprintf(upload.metadata_path.data(), upload.metadata_path.size(), "%s.rme-meta", path->data()) >= static_cast<int>(upload.metadata_path.size())
                || !parse_sha256(*sha, upload.expected_sha) || !make_dirs(upload.partial_path.data())
                || !(upload.file = fopen(upload.partial_path.data(), "wb"))
                || setvbuf(upload.file, upload_file_buffer.data(), _IOFBF, upload_file_buffer.size()) != 0) {
                reset_upload(true);
                report_error("open_failed");
            } else {
                upload.expected_size = *size;
                upload.bulk = bulk;
                upload.binary = binary;
                upload.cdc_connected_at_begin = tud_cdc_connected();
                upload.last_activity_ms = ticks_ms();
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, 0, *size);
                mbedtls_sha256_init(&upload.sha);
                upload.sha_initialized = true;
                if (mbedtls_sha256_starts_ret(&upload.sha, false)) {
                    reset_upload(true);
                    report_error("hash_failed");
                } else if (!persist_upload_metadata()) {
                    reset_upload(true);
                    report_error("metadata_write_failed");
                } else if (binary) {
                    SERIAL_ECHOPGM("RME_FILE_BINARY_READY offset=0 chunk=");
                    SERIAL_ECHO(binary_chunk_size);
                    SERIAL_ECHOPGM(" window=");
                    SERIAL_ECHO(binary_window_size);
                    SERIAL_ECHOLNPGM(" header=10 endian=little crc=crc32");
                } else if (bulk) {
                    SERIAL_ECHOLNPGM("RME_FILE_BULK_READY offset=0 chunk=384 window=4");
                } else {
                    SERIAL_ECHOLNPGM("RME_FILE_WRITE_READY offset=0 chunk=48");
                }
            }
        }
    } else if (action_is(action, "WRITE_CHUNK") || action_is(action, "WRITE_BULK_CHUNK")) {
        const bool bulk = action_is(action, "WRITE_BULK_CHUNK");
        const auto offset = number(command, "offset");
        const auto data = value(command, "data");
        if (!upload.file || upload.bulk != bulk || !offset || *offset != upload.received || !data) {
            report_error("upload_state");
        } else {
            size_t count = 0;
            const char *failure = nullptr;
            if (!bulk && data->size() > 64) {
                failure = "chunk_too_large";
            } else if (mbedtls_base64_decode(binary_payload.data(), bulk ? bulk_chunk_size : transfer_chunk_size, &count, reinterpret_cast<const unsigned char *>(data->data()), data->size())) {
                failure = "decode_failed";
            } else if (count > upload.expected_size - upload.received) {
                failure = "size_exceeded";
            } else if (fwrite(binary_payload.data(), 1, count, upload.file) != count) {
                failure = "disk_write_failed";
            } else if (mbedtls_sha256_update_ret(&upload.sha, binary_payload.data(), count)) {
                failure = "hash_failed";
            }
            if (failure) {
                suspend_upload(failure == std::string_view("disk_write_failed")
                        ? transfers::Monitor::Outcome::ErrorStorage
                        : transfers::Monitor::Outcome::ErrorOther);
                report_upload_error(failure, true);
            } else {
                upload.received += count;
                upload.last_activity_ms = ticks_ms();
                if (upload_slot) {
                    upload_slot->progress(count);
                }
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, upload.received, upload.expected_size);
                if (!bulk) {
                    SERIAL_ECHOPGM("RME_FILE_WRITE_OFFSET offset=");
                    SERIAL_ECHOLN(upload.received);
                } else if (++upload.unacknowledged >= bulk_window_size || upload.received == upload.expected_size) {
                    upload.unacknowledged = 0;
                    SERIAL_ECHOPGM("RME_FILE_BULK_ACK offset=");
                    SERIAL_ECHOLN(upload.received);
                }
            }
        }
    } else if (action_is(action, "WRITE_END") || action_is(action, "WRITE_BULK_END")) {
        const bool bulk = action_is(action, "WRITE_BULK_END");
        if (!upload.file || upload.bulk != bulk || upload.received != upload.expected_size) {
            report_error("size_mismatch");
        } else {
            std::array<uint8_t, 32> actual {};
            bool ok = fflush(upload.file) == 0 && fsync(fileno(upload.file)) == 0
                && mbedtls_sha256_finish_ret(&upload.sha, actual.data()) == 0 && actual == upload.expected_sha;
            fclose(upload.file);
            upload.file = nullptr;
            mbedtls_sha256_free(&upload.sha);
            upload.sha_initialized = false;
            if (ok) {
                ok = replace_uploaded_file(upload.partial_path.data(), upload.final_path.data());
            }
            if (!ok) {
                reset_upload(true);
                report_error("finalize_failed");
            } else {
                cache_completed_firmware_upload();
                SERIAL_ECHOPGM("RME_FILE_WRITE_COMPLETE path=");
                SERIAL_ECHOLN(upload.final_path.data() + 5);
                reset_upload(false, transfers::Monitor::Outcome::Finished);
            }
        }
    } else if (action_is(action, "CRASH_DUMP")) {
        if (!marlin_server::printer_idle()) {
            report_error("printer_busy");
        } else if (!crash_dump::dump_is_valid()) {
            report_error("no_crash_dump");
        } else if (auto slot = transfers::Monitor::instance.allocate(
                       transfers::Monitor::Type::Link, path->data(), 0)) {
            const bool saved = crash_dump::save_dump_to_usb(path->data());
            slot->done(saved ? transfers::Monitor::Outcome::Finished
                             : transfers::Monitor::Outcome::ErrorStorage);
            if (!saved) {
                report_error("crash_dump_write_failed");
            } else {
                SERIAL_ECHOPGM("RME_FILE_CRASH_DUMP_SAVED path=");
                SERIAL_ECHOLN(path->data() + 5);
            }
        } else {
            report_error("transfer_busy");
        }
    } else if (action_is(action, "DELETE")) {
        if (transfers::Monitor::instance.id().has_value()) {
            report_error("transfer_busy");
        } else if (is_firmware_candidate(*path)) {
            report_error("protected_firmware");
        } else if (!marlin_server::printer_idle() || remove(path->data())) {
            report_error("delete_failed");
        } else {
            SERIAL_ECHOLNPGM("RME_FILE_DELETED");
        }
    } else if (action_is(action, "RENAME")) {
        const auto destination = path_value(command, "dest");
        if (transfers::Monitor::instance.id().has_value()) {
            report_error("transfer_busy");
        } else if (is_firmware_candidate(*path) || (destination && is_firmware_candidate(*destination))) {
            report_error("protected_firmware");
        } else if (!destination || !root_allowed(*destination, action) || !marlin_server::printer_idle() || rename(path->data(), destination->data())) {
            report_error("rename_failed");
        } else {
            SERIAL_ECHOLNPGM("RME_FILE_RENAMED");
        }
    } else if (action_is(action, "MKDIR")) {
        if (transfers::Monitor::instance.id().has_value()) {
            report_error("transfer_busy");
        } else if (!marlin_server::printer_idle() || mkdir(path->data(), 0777)) {
            report_error("mkdir_failed");
        } else {
            SERIAL_ECHOLNPGM("RME_FILE_DIRECTORY_CREATED");
        }
    } else if (action_is(action, "PRINT") || action_is(action, "FLASH")) {
        const bool flash = action_is(action, "FLASH");
        const char *extension = strrchr(path->data(), '.');
        if (transfers::Monitor::instance.id().has_value()) {
            report_error("transfer_busy");
        } else if (!marlin_server::printer_idle()) {
            report_error("printer_busy");
        } else if (flash && (!extension || (strcasecmp(extension, ".bbf") && strcasecmp(path->data(), "/usb/FWUPD.RME")))) {
            report_error("invalid_firmware");
        } else {
            std::array<char, MAX_CMD_SIZE> gcode {};
            const int count = snprintf(gcode.data(), gcode.size(), flash ? "M997 %s" : "M32 %s", path->data());
            if (count <= 0 || count >= static_cast<int>(gcode.size()) || !queue.enqueue_one(gcode.data(), false)) {
                report_error("queue_full");
            } else {
                SERIAL_ECHOLNPGM(flash ? "RME_FILE_FLASH_QUEUED" : "RME_FILE_PRINT_QUEUED");
            }
        }
    } else {
        return false;
    }
    return true;
}

extern "C" bool buddy_rme_firmware_service(const char *raw_command) {
    constexpr std::string_view prefix = "@RME FIRMWARE ";
    const std::string_view command { raw_command, strcspn(raw_command, "*") };
    if (!command.starts_with(prefix)) {
        return false;
    }
    const auto action = command.substr(prefix.size());
    const auto action_is = [](const std::string_view candidate, const std::string_view expected) {
        return rme_protocol::action_is(candidate, expected);
    };

    if (action_is(action, "QUERY")) {
        if (upload.file || transfers::Monitor::instance.id().has_value()) {
            SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=transfer_busy");
        } else {
            report_firmware_status();
        }
        return true;
    }
    if (action_is(action, "UNSTAGE")) {
        if (firmware_armed()) {
            SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=firmware_armed");
            return true;
        }
        if (!marlin_server::printer_idle()) {
            SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=printer_busy");
            return true;
        }
        auto slot = transfers::Monitor::instance.allocate(
            transfers::Monitor::Type::Link, firmware_candidate_path, 0);
        if (!slot) {
            SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=transfer_busy");
            return true;
        }
        stop_firmware_validation();
        if (upload.suspended && strcmp(upload.final_path.data(), firmware_candidate_path) == 0) {
            reset_upload(true, transfers::Monitor::Outcome::Stopped, false);
        }
        constexpr const char *paths[] = {
            "/usb/FWUPD.RME",
            "/usb/FWUPD.RME.rme-part",
            "/usb/FWUPD.RME.rme-meta",
            "/usb/FWUPD.RME.rme-tmp",
            "/usb/FWUPD.RME.rme-old",
            "/usb/FWUPD.RME.rme-verified",
            "/usb/FWUPD.RME.rme-verified-tmp",
            "/usb/FWUPD.UI",
        };
        bool ok = true;
        for (const char *path : paths) {
            errno = 0;
            if (remove(path) != 0 && errno != ENOENT) {
                ok = false;
            }
        }
        slot->done(ok ? transfers::Monitor::Outcome::Finished : transfers::Monitor::Outcome::ErrorStorage);
        if (ok) {
            verified_firmware_cache = {};
            verified_firmware_cache_loaded = false;
            SERIAL_ECHOLNPGM("RME_FIRMWARE_UNSTAGED candidate=0 armed=0");
        } else {
            SERIAL_ECHOLNPGM("echo:RME_ERROR workflow=firmware code=unstage_failed");
        }
        return true;
    }
    return false;
}

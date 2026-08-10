#include <Marlin/src/core/serial.h>
#include <Marlin/src/gcode/queue.h>
#include <USBSerial.h>

#include <common/directory.hpp>
#include <common/filename_type.hpp>
#include <common/path_utils.h>
#include <marlin_server.hpp>
#include <serial_remote_control.hpp>
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

namespace {
constexpr size_t transfer_chunk_size = 48;
constexpr size_t bulk_chunk_size = 384;
constexpr uint8_t bulk_window_size = 4;
// USB CDC is a byte stream using 64-byte full-speed endpoint packets. A 1 KiB
// application frame keeps the endpoint busy without permanently consuming
// several large SRAM blocks. Hosts pipeline frames using the advertised
// window, so this changes framing overhead by less than one percent.
constexpr size_t binary_chunk_size = 1024;
constexpr uint8_t binary_window_size = 8;
constexpr uint32_t maximum_file_size = 1024U * 1024U * 1024U;

struct UploadState {
    FILE *file = nullptr;
    uint32_t expected_size = 0;
    uint32_t received = 0;
    std::array<uint8_t, 32> expected_sha {};
    mbedtls_sha256_context sha {};
    bool sha_initialized = false;
    std::array<char, FILE_PATH_BUFFER_LEN> final_path {};
    std::array<char, FILE_PATH_BUFFER_LEN> partial_path {};
    bool bulk = false;
    bool binary = false;
    uint8_t unacknowledged = 0;
} upload;

struct BinaryReceiver {
    std::array<uint8_t, 10> header {};
    uint16_t header_received = 0;
    uint16_t payload_received = 0;
    uint16_t payload_size = 0;
    uint32_t offset = 0;
    uint32_t expected_crc = 0;
    uint8_t unacknowledged = 0;
    uint16_t discard_remaining = 0;
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

uint16_t read_le16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t read_le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
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

std::optional<std::string_view> value(const std::string_view command, const std::string_view key) {
    size_t token = command.find(' ');
    while (token != std::string_view::npos) {
        ++token;
        const size_t end = command.find_first_of(" *", token);
        const size_t count = (end == std::string_view::npos ? command.size() : end) - token;
        if (count > key.size() && command[token + key.size()] == '=' && command.substr(token, key.size()) == key)
            return command.substr(token + key.size() + 1, count - key.size() - 1);
        token = end;
    }
    return std::nullopt;
}

std::optional<uint32_t> number(const std::string_view command, const std::string_view key) {
    const auto text = value(command, key);
    if (!text || text->empty() || text->size() >= 16) return std::nullopt;
    std::array<char, 16> buffer {};
    std::copy(text->begin(), text->end(), buffer.begin());
    char *end = nullptr;
    const unsigned long parsed = strtoul(buffer.data(), &end, 10);
    if (end == buffer.data() || *end || parsed > UINT32_MAX) return std::nullopt;
    return static_cast<uint32_t>(parsed);
}

int hex_nibble(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool decode_path(const std::string_view encoded, std::array<char, FILE_PATH_BUFFER_LEN> &path) {
    constexpr std::string_view root = "/usb/";
    size_t write = 0;
    for (const char c : root) path[write++] = c;
    size_t read = 0;
    while (read < encoded.size() && encoded[read] == '/') ++read;
    while (read < encoded.size()) {
        char c = encoded[read++];
        if (c == '%' && read + 1 < encoded.size()) {
            const int high = hex_nibble(encoded[read]);
            const int low = hex_nibble(encoded[read + 1]);
            if (high < 0 || low < 0) return false;
            c = static_cast<char>((high << 4) | low);
            read += 2;
        }
        if (!c || write + 1 >= path.size()) return false;
        path[write++] = c;
    }
    path[write] = '\0';
    const char *relative = path.data() + root.size();
    return !strstr(relative, "..") && !strstr(relative, "//");
}

std::optional<std::array<char, FILE_PATH_BUFFER_LEN>> path_value(const std::string_view command, const std::string_view key = "path") {
    const auto encoded = value(command, key);
    std::array<char, FILE_PATH_BUFFER_LEN> path {};
    if (!encoded || !decode_path(*encoded, path)) return std::nullopt;
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
    if (text.size() != 64) return false;
    for (size_t i = 0; i < result.size(); ++i) {
        const int high = hex_nibble(text[i * 2]);
        const int low = hex_nibble(text[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        result[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

void reset_upload(const bool remove_partial) {
    if (upload.file) fclose(upload.file);
    if (upload.sha_initialized) mbedtls_sha256_free(&upload.sha);
    if (remove_partial && upload.partial_path[0]) remove(upload.partial_path.data());
    upload = {};
    binary_receiver = {};
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
}

bool root_allowed(const std::array<char, FILE_PATH_BUFFER_LEN> &path, const std::string_view action) {
    return path[5] != '\0' || action.starts_with("LIST") || action.starts_with("STAT");
}

// FatFS does not consistently provide POSIX rename-over-existing semantics.
// Preserve the previous file until the verified partial upload has taken its
// place so every upload mode can safely replace an existing destination.
bool replace_uploaded_file(const char *partial_path, const char *final_path) {
    std::array<char, FILE_PATH_BUFFER_LEN> backup_path {};
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
    fclose(upload.file); upload.file = nullptr;
    mbedtls_sha256_free(&upload.sha); upload.sha_initialized = false;
    if (ok) ok = replace_uploaded_file(upload.partial_path.data(), upload.final_path.data());
    if (!ok) {
        reset_upload(true);
        report_error("finalize_failed");
        return false;
    }
    SERIAL_ECHOPGM(complete_prefix); SERIAL_ECHOLN(upload.final_path.data() + 5);
    reset_upload(false);
    return true;
}
} // namespace

extern "C" bool buddy_rme_binary_upload_active() {
    return upload.file && upload.binary;
}

extern "C" void buddy_rme_binary_upload_byte(const uint8_t byte) {
    if (!buddy_rme_binary_upload_active()) return;

    if (binary_receiver.discard_remaining) {
        if (--binary_receiver.discard_remaining == 0) {
            const uint8_t unacknowledged = binary_receiver.unacknowledged;
            binary_receiver = {};
            binary_receiver.unacknowledged = unacknowledged;
        }
        return;
    }

    if (binary_receiver.header_received < binary_receiver.header.size()) {
        binary_receiver.header[binary_receiver.header_received++] = byte;
        if (binary_receiver.header_received != binary_receiver.header.size()) return;
        binary_receiver.offset = read_le32(binary_receiver.header.data());
        binary_receiver.payload_size = read_le16(binary_receiver.header.data() + 4);
        binary_receiver.expected_crc = read_le32(binary_receiver.header.data() + 6);

        if (binary_receiver.payload_size == 0) {
            if (binary_receiver.offset == UINT32_MAX) {
                reset_upload(true);
                SERIAL_ECHOLNPGM("RME_FILE_BINARY_ABORTED");
            } else if (binary_receiver.offset == upload.received && upload.received == upload.expected_size) {
                finish_current_upload("RME_FILE_BINARY_COMPLETE path=");
            } else {
                SERIAL_ECHOPGM("RME_FILE_BINARY_NACK offset="); SERIAL_ECHOLN(upload.received);
                binary_receiver = {};
            }
            return;
        }
        if (binary_receiver.payload_size > binary_chunk_size) {
            SERIAL_ECHOPGM("RME_FILE_BINARY_NACK offset="); SERIAL_ECHOLN(upload.received);
            binary_receiver.discard_remaining = binary_receiver.payload_size;
        }
        return;
    }

    binary_payload[binary_receiver.payload_received++] = byte;
    if (binary_receiver.payload_received != binary_receiver.payload_size) return;

    const bool valid = binary_receiver.offset == upload.received
        && binary_receiver.payload_size <= upload.expected_size - upload.received
        && crc32_sw(binary_payload.data(), binary_receiver.payload_size, 0) == binary_receiver.expected_crc;
    if (!valid
        || fwrite(binary_payload.data(), 1, binary_receiver.payload_size, upload.file) != binary_receiver.payload_size
        || mbedtls_sha256_update_ret(&upload.sha, binary_payload.data(), binary_receiver.payload_size)) {
        SERIAL_ECHOPGM("RME_FILE_BINARY_NACK offset="); SERIAL_ECHOLN(upload.received);
        binary_receiver = {};
        return;
    }

    upload.received += binary_receiver.payload_size;
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, upload.received, upload.expected_size);
    const bool acknowledge = ++binary_receiver.unacknowledged >= binary_window_size || upload.received == upload.expected_size;
    const uint8_t unacknowledged = binary_receiver.unacknowledged;
    binary_receiver = {};
    binary_receiver.unacknowledged = acknowledge ? 0 : unacknowledged;
    if (acknowledge) {
        SERIAL_ECHOPGM("RME_FILE_BINARY_ACK offset="); SERIAL_ECHOLN(upload.received);
    }
}

extern "C" bool buddy_rme_file_service(const char *raw_command) {
    constexpr std::string_view prefix = "@RME FILE ";
    const std::string_view command { raw_command, strcspn(raw_command, "*") };
    if (!command.starts_with(prefix)) return false;
    const auto action = command.substr(prefix.size());

    if (action.starts_with("CAPS")) {
        SERIAL_ECHOLNPGM("RME_FILE_CAPS root=/usb chunk=48 bulk=1 bulk_chunk=384 bulk_window=4 binary=1 binary_chunk=1024 binary_window=8 binary_read=1 binary_read_chunk=1024 max_size=1073741824 list=1 stat=1 read=1 write=1 overwrite=1 delete=1 rename=1 mkdir=1 print=1 flash=1");
        return true;
    }
    if (action.starts_with("ABORT")) {
        reset_upload(true);
        SERIAL_ECHOLNPGM("RME_FILE_ABORTED");
        return true;
    }

    const bool state_only = action.starts_with("WRITE_CHUNK") || action.starts_with("WRITE_END")
        || action.starts_with("WRITE_BULK_CHUNK") || action.starts_with("WRITE_BULK_END");
    const auto path = state_only ? std::optional<std::array<char, FILE_PATH_BUFFER_LEN>> {} : path_value(command);
    if (!state_only && (!path || !root_allowed(*path, action))) {
        report_error(path ? "root_protected" : "invalid_path");
        return true;
    }

    if (action.starts_with("STAT")) {
        struct stat st {};
        if (stat(path->data(), &st)) report_error("not_found");
        else {
            SERIAL_ECHOPGM("RME_FILE_STAT path="); SERIAL_ECHO(path->data() + 5);
            SERIAL_ECHOPGM(" type="); SERIAL_ECHO(S_ISDIR(st.st_mode) ? "dir" : "file");
            SERIAL_ECHOPGM(" size="); SERIAL_ECHO(static_cast<uint32_t>(st.st_size));
            SERIAL_ECHOPGM(" mtime="); SERIAL_ECHOLN(static_cast<uint32_t>(st.st_mtime));
        }
    } else if (action.starts_with("LIST")) {
        Directory directory(path->data());
        if (!directory) report_error("not_directory");
        else {
            while (const dirent *entry = directory.read()) {
                if (!entry->d_name[0] || entry->d_name[0] == '.') continue;
                std::array<char, FILE_PATH_BUFFER_LEN> child {};
                const size_t path_length = strlen(path->data());
                if (snprintf(child.data(), child.size(), "%s%s%s", path->data(), path_length && (*path)[path_length - 1] == '/' ? "" : "/", entry->d_name) >= static_cast<int>(child.size())) continue;
                struct stat st {};
                if (stat(child.data(), &st)) continue;
                SERIAL_ECHOPGM("RME_FILE_ENTRY name="); SERIAL_ECHO(entry->d_name);
                SERIAL_ECHOPGM(" type="); SERIAL_ECHO(S_ISDIR(st.st_mode) ? "dir" : "file");
                SERIAL_ECHOPGM(" size="); SERIAL_ECHOLN(static_cast<uint32_t>(st.st_size));
            }
            SERIAL_ECHOLNPGM("RME_FILE_LIST_END");
        }
    } else if (action.starts_with("READ_BINARY")) {
        const uint32_t offset = number(command, "offset").value_or(0);
        const uint32_t length = number(command, "length").value_or(binary_chunk_size);
        struct stat st {};
        if (!length || length > binary_chunk_size || offset > maximum_file_size) report_error("invalid_range");
        else if (stat(path->data(), &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || static_cast<uint64_t>(st.st_size) > maximum_file_size) report_error("read_failed");
        else if (FILE *file = fopen(path->data(), "rb")) {
            // The application buffer already bounds this read. Do not let
            // newlib allocate a second hidden stream buffer from the heap.
            setvbuf(file, nullptr, _IONBF, 0);
            if (fseek(file, offset, SEEK_SET)) { fclose(file); report_error("read_failed"); return true; }
            const size_t count = fread(binary_payload.data(), 1, length, file);
            const bool read_error = ferror(file);
            fclose(file);
            if (read_error) { report_error("read_failed"); return true; }

            std::array<uint8_t, 10> header {};
            write_le32(header.data(), offset);
            write_le16(header.data() + 4, static_cast<uint16_t>(count));
            write_le32(header.data() + 6, crc32_sw(binary_payload.data(), count, 0));
            const uint32_t file_size = static_cast<uint32_t>(st.st_size);
            const uint32_t next = offset + count;
            const bool eof = next >= file_size;
            serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, std::min(next, file_size), file_size);
            SERIAL_ECHOPGM("RME_FILE_BINARY_READ_READY path="); SERIAL_ECHO(path->data() + 5);
            SERIAL_ECHOPGM(" offset="); SERIAL_ECHO(offset);
            SERIAL_ECHOPGM(" length="); SERIAL_ECHO(count);
            SERIAL_ECHOLNPGM(" header=10 endian=little crc=crc32");
            SERIAL_FLUSHTX();
            SerialUSB.cdc_write_sync(header.data(), header.size());
            if (count) SerialUSB.cdc_write_sync(binary_payload.data(), count);
            SERIAL_ECHOPGM("RME_FILE_BINARY_READ_COMPLETE next="); SERIAL_ECHO(next);
            SERIAL_ECHOPGM(" eof="); SERIAL_ECHOLN(eof ? 1 : 0);
            if (eof) serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
        } else report_error("read_failed");
    } else if (action.starts_with("READ")) {
        const uint32_t offset = number(command, "offset").value_or(0);
        const uint32_t length = number(command, "length").value_or(transfer_chunk_size);
        if (!length || length > transfer_chunk_size) report_error("invalid_range");
        else if (FILE *file = fopen(path->data(), "rb")) {
            setvbuf(file, nullptr, _IONBF, 0);
            std::array<uint8_t, transfer_chunk_size> raw {};
            if (fseek(file, offset, SEEK_SET)) { fclose(file); report_error("read_failed"); return true; }
            const size_t count = fread(raw.data(), 1, length, file);
            const bool eof = feof(file);
            fclose(file);
            std::array<unsigned char, 65> encoded {};
            size_t encoded_size = 0;
            if (mbedtls_base64_encode(encoded.data(), encoded.size(), &encoded_size, raw.data(), count)) report_error("encode_failed");
            else {
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file);
                SERIAL_ECHOPGM("RME_FILE_DATA path="); SERIAL_ECHO(path->data() + 5);
                SERIAL_ECHOPGM(" offset="); SERIAL_ECHO(offset);
                SERIAL_ECHOPGM(" length="); SERIAL_ECHO(count);
                SERIAL_ECHOPGM(" eof="); SERIAL_ECHO(eof ? 1 : 0);
                SERIAL_ECHOPGM(" data="); SERIAL_ECHOLN(reinterpret_cast<char *>(encoded.data()));
            }
        } else report_error("read_failed");
    } else if (action.starts_with("WRITE_BEGIN") || action.starts_with("WRITE_BULK_BEGIN") || action.starts_with("WRITE_BINARY_BEGIN")) {
        const bool bulk = action.starts_with("WRITE_BULK_BEGIN");
        const bool binary = action.starts_with("WRITE_BINARY_BEGIN");
        const auto size = number(command, "size");
        const auto sha = value(command, "sha256");
        if (!marlin_server::printer_idle()) report_error("printer_busy");
        else if (!size || *size > maximum_file_size || !sha) report_error("invalid_upload");
        else {
            reset_upload(true);
            upload.final_path = *path;
            if (snprintf(upload.partial_path.data(), upload.partial_path.size(), "%s.rme-part", path->data()) >= static_cast<int>(upload.partial_path.size())
                || !parse_sha256(*sha, upload.expected_sha) || !make_dirs(upload.partial_path.data())
                || !(upload.file = fopen(upload.partial_path.data(), "wb"))
                || setvbuf(upload.file, upload_file_buffer.data(), _IOFBF, upload_file_buffer.size()) != 0) {
                reset_upload(true); report_error("open_failed");
            } else {
                upload.expected_size = *size;
                upload.bulk = bulk;
                upload.binary = binary;
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, 0, *size);
                mbedtls_sha256_init(&upload.sha);
                upload.sha_initialized = true;
                if (mbedtls_sha256_starts_ret(&upload.sha, false)) { reset_upload(true); report_error("hash_failed"); }
                else if (binary) SERIAL_ECHOLNPGM("RME_FILE_BINARY_READY offset=0 chunk=1024 window=8 header=10 endian=little crc=crc32");
                else if (bulk) SERIAL_ECHOLNPGM("RME_FILE_BULK_READY offset=0 chunk=384 window=4");
                else SERIAL_ECHOLNPGM("RME_FILE_WRITE_READY offset=0 chunk=48");
            }
        }
    } else if (action.starts_with("WRITE_CHUNK") || action.starts_with("WRITE_BULK_CHUNK")) {
        const bool bulk = action.starts_with("WRITE_BULK_CHUNK");
        const auto offset = number(command, "offset");
        const auto data = value(command, "data");
        if (!upload.file || upload.bulk != bulk || !offset || *offset != upload.received || !data) report_error("upload_state");
        else {
            std::array<uint8_t, bulk_chunk_size> decoded {};
            size_t count = 0;
            if ((!bulk && data->size() > 64)
                || mbedtls_base64_decode(decoded.data(), bulk ? bulk_chunk_size : transfer_chunk_size, &count, reinterpret_cast<const unsigned char *>(data->data()), data->size())
                || count > upload.expected_size - upload.received || fwrite(decoded.data(), 1, count, upload.file) != count
                || mbedtls_sha256_update_ret(&upload.sha, decoded.data(), count)) {
                reset_upload(true); report_error("write_failed");
            } else {
                upload.received += count;
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, upload.received, upload.expected_size);
                if (!bulk) {
                    SERIAL_ECHOPGM("RME_FILE_WRITE_OFFSET offset="); SERIAL_ECHOLN(upload.received);
                } else if (++upload.unacknowledged >= bulk_window_size || upload.received == upload.expected_size) {
                    upload.unacknowledged = 0;
                    SERIAL_ECHOPGM("RME_FILE_BULK_ACK offset="); SERIAL_ECHOLN(upload.received);
                }
            }
        }
    } else if (action.starts_with("WRITE_END") || action.starts_with("WRITE_BULK_END")) {
        const bool bulk = action.starts_with("WRITE_BULK_END");
        if (!upload.file || upload.bulk != bulk || upload.received != upload.expected_size) report_error("size_mismatch");
        else {
            std::array<uint8_t, 32> actual {};
            bool ok = fflush(upload.file) == 0 && fsync(fileno(upload.file)) == 0
                && mbedtls_sha256_finish_ret(&upload.sha, actual.data()) == 0 && actual == upload.expected_sha;
            fclose(upload.file); upload.file = nullptr;
            mbedtls_sha256_free(&upload.sha); upload.sha_initialized = false;
            if (ok) ok = replace_uploaded_file(upload.partial_path.data(), upload.final_path.data());
            if (!ok) { reset_upload(true); report_error("finalize_failed"); }
            else {
                SERIAL_ECHOPGM("RME_FILE_WRITE_COMPLETE path="); SERIAL_ECHOLN(upload.final_path.data() + 5);
                reset_upload(false);
            }
        }
    } else if (action.starts_with("DELETE")) {
        if (!marlin_server::printer_idle() || remove(path->data())) report_error("delete_failed");
        else SERIAL_ECHOLNPGM("RME_FILE_DELETED");
    } else if (action.starts_with("RENAME")) {
        const auto destination = path_value(command, "dest");
        if (!destination || !root_allowed(*destination, action) || !marlin_server::printer_idle() || rename(path->data(), destination->data())) report_error("rename_failed");
        else SERIAL_ECHOLNPGM("RME_FILE_RENAMED");
    } else if (action.starts_with("MKDIR")) {
        if (mkdir(path->data(), 0777)) report_error("mkdir_failed");
        else SERIAL_ECHOLNPGM("RME_FILE_DIRECTORY_CREATED");
    } else if (action.starts_with("PRINT") || action.starts_with("FLASH")) {
        const bool flash = action.starts_with("FLASH");
        const char *extension = strrchr(path->data(), '.');
        if (!marlin_server::printer_idle()) report_error("printer_busy");
        else if (flash && (!extension || (strcasecmp(extension, ".bbf") && strcasecmp(path->data(), "/usb/FWUPD.RME")))) report_error("invalid_firmware");
        else {
            std::array<char, MAX_CMD_SIZE> gcode {};
            const int count = snprintf(gcode.data(), gcode.size(), flash ? "M997 %s" : "M32 %s", path->data());
            if (count <= 0 || count >= static_cast<int>(gcode.size()) || !queue.enqueue_one(gcode.data(), false)) report_error("queue_full");
            else SERIAL_ECHOLNPGM(flash ? "RME_FILE_FLASH_QUEUED" : "RME_FILE_PRINT_QUEUED");
        }
    } else return false;
    return true;
}

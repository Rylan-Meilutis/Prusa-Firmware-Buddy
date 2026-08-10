#include <Marlin/src/core/serial.h>
#include <Marlin/src/gcode/queue.h>

#include <common/directory.hpp>
#include <common/filename_type.hpp>
#include <common/path_utils.h>
#include <marlin_server.hpp>
#include <serial_remote_control.hpp>

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
} upload;

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
    serial_remote_control::set_transfer(serial_remote_control::TransferKind::none);
}

bool root_allowed(const std::array<char, FILE_PATH_BUFFER_LEN> &path, const std::string_view action) {
    return path[5] != '\0' || action.starts_with("LIST") || action.starts_with("STAT");
}
} // namespace

extern "C" bool buddy_rme_file_service(const char *raw_command) {
    constexpr std::string_view prefix = "@RME FILE ";
    const std::string_view command { raw_command, strcspn(raw_command, "*") };
    if (!command.starts_with(prefix)) return false;
    const auto action = command.substr(prefix.size());

    if (action.starts_with("CAPS")) {
        SERIAL_ECHOLNPGM("RME_FILE_CAPS root=/usb chunk=48 max_size=1073741824 list=1 stat=1 read=1 write=1 delete=1 rename=1 mkdir=1 print=1 flash=1");
        return true;
    }
    if (action.starts_with("ABORT")) {
        reset_upload(true);
        SERIAL_ECHOLNPGM("RME_FILE_ABORTED");
        return true;
    }

    const auto path = path_value(command);
    if (!path || !root_allowed(*path, action)) {
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
    } else if (action.starts_with("READ")) {
        const uint32_t offset = number(command, "offset").value_or(0);
        const uint32_t length = number(command, "length").value_or(transfer_chunk_size);
        if (!length || length > transfer_chunk_size) report_error("invalid_range");
        else if (FILE *file = fopen(path->data(), "rb")) {
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
    } else if (action.starts_with("WRITE_BEGIN")) {
        const auto size = number(command, "size");
        const auto sha = value(command, "sha256");
        if (!marlin_server::printer_idle()) report_error("printer_busy");
        else if (!size || *size > maximum_file_size || !sha) report_error("invalid_upload");
        else {
            reset_upload(true);
            upload.final_path = *path;
            if (snprintf(upload.partial_path.data(), upload.partial_path.size(), "%s.rme-part", path->data()) >= static_cast<int>(upload.partial_path.size())
                || !parse_sha256(*sha, upload.expected_sha) || !make_dirs(upload.partial_path.data())
                || !(upload.file = fopen(upload.partial_path.data(), "wb"))) {
                reset_upload(true); report_error("open_failed");
            } else {
                upload.expected_size = *size;
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, 0, *size);
                mbedtls_sha256_init(&upload.sha);
                upload.sha_initialized = true;
                if (mbedtls_sha256_starts_ret(&upload.sha, false)) { reset_upload(true); report_error("hash_failed"); }
                else SERIAL_ECHOLNPGM("RME_FILE_WRITE_READY offset=0 chunk=48");
            }
        }
    } else if (action.starts_with("WRITE_CHUNK")) {
        const auto offset = number(command, "offset");
        const auto data = value(command, "data");
        if (!upload.file || !offset || *offset != upload.received || !data) report_error("upload_state");
        else {
            std::array<uint8_t, transfer_chunk_size> decoded {};
            size_t count = 0;
            if (mbedtls_base64_decode(decoded.data(), decoded.size(), &count, reinterpret_cast<const unsigned char *>(data->data()), data->size())
                || count > upload.expected_size - upload.received || fwrite(decoded.data(), 1, count, upload.file) != count
                || mbedtls_sha256_update_ret(&upload.sha, decoded.data(), count)) {
                reset_upload(true); report_error("write_failed");
            } else {
                upload.received += count;
                serial_remote_control::set_transfer(serial_remote_control::TransferKind::file, upload.received, upload.expected_size);
                SERIAL_ECHOPGM("RME_FILE_WRITE_OFFSET offset="); SERIAL_ECHOLN(upload.received);
            }
        }
    } else if (action.starts_with("WRITE_END")) {
        if (!upload.file || upload.received != upload.expected_size) report_error("size_mismatch");
        else {
            std::array<uint8_t, 32> actual {};
            bool ok = fflush(upload.file) == 0 && fsync(fileno(upload.file)) == 0
                && mbedtls_sha256_finish_ret(&upload.sha, actual.data()) == 0 && actual == upload.expected_sha;
            fclose(upload.file); upload.file = nullptr;
            mbedtls_sha256_free(&upload.sha); upload.sha_initialized = false;
            if (ok) ok = rename(upload.partial_path.data(), upload.final_path.data()) == 0;
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
        else if (flash && (!extension || strcasecmp(extension, ".bbf"))) report_error("invalid_firmware");
        else {
            std::array<char, MAX_CMD_SIZE> gcode {};
            const int count = snprintf(gcode.data(), gcode.size(), flash ? "M997 %s" : "M32 %s", path->data());
            if (count <= 0 || count >= static_cast<int>(gcode.size()) || !queue.enqueue_one(gcode.data(), false)) report_error("queue_full");
            else SERIAL_ECHOLNPGM(flash ? "RME_FILE_FLASH_QUEUED" : "RME_FILE_PRINT_QUEUED");
        }
    } else return false;
    return true;
}

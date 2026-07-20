#include "gcode/gcode.h"
#include "PrusaGcodeSuite.hpp"

#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {
constexpr const char *temporary_path = "/usb/FWUPD.TMP";
constexpr const char *firmware_path = "/usb/FWUPD.BBF";
constexpr uint32_t maximum_size = 32U * 1024U * 1024U;
constexpr size_t maximum_chunk_size = 48;

struct UploadState {
    bool active = false;
    bool sha_initialized = false;
    uint32_t expected_size = 0;
    uint32_t received = 0;
    std::array<uint8_t, 32> expected_sha {};
    mbedtls_sha256_context sha;
} upload;

void reset_state(bool remove_temporary) {
    if (upload.sha_initialized) {
        mbedtls_sha256_free(&upload.sha);
    }
    upload = {};
    if (remove_temporary) {
        remove(temporary_path);
    }
}

const char *value_after(const char *body, char key) {
    while (*body) {
        while (*body == ' ') ++body;
        if (*body == key && body[1] != ' ' && body[1] != '\0') return body + 1;
        while (*body && *body != ' ') ++body;
    }
    return nullptr;
}

bool parse_u32(const char *value, uint32_t &result) {
    if (!value) return false;
    char *end = nullptr;
    const unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || (*end != ' ' && *end != '\0') || parsed > UINT32_MAX) return false;
    result = parsed;
    return true;
}

uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return 0xff;
}

bool parse_sha(const char *value, std::array<uint8_t, 32> &result) {
    if (!value) return false;
    for (size_t index = 0; index < result.size(); ++index) {
        const uint8_t high = hex_nibble(value[index * 2]);
        const uint8_t low = hex_nibble(value[index * 2 + 1]);
        if (high == 0xff || low == 0xff) {
            return false;
        }
        result[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return value[result.size() * 2] == ' ' || value[result.size() * 2] == '\0';
}

void report_error(const char *message, bool abort_upload = false) {
    SERIAL_ERROR_START();
    SERIAL_ECHOLNPAIR("FW_UPLOAD ", message);
    if (abort_upload) {
        reset_state(true);
    }
}

void begin_upload(const char *body) {
    uint32_t size = 0;
    std::array<uint8_t, 32> sha {};
    if (!parse_u32(value_after(body, 'S'), size) || size == 0 || size > maximum_size
        || !parse_sha(value_after(body, 'H'), sha)) {
        report_error("BEGIN");
        return;
    }

    reset_state(true);
    FILE *file = fopen(temporary_path, "wb");
    if (!file) {
        report_error("OPEN");
        return;
    }
    const bool close_ok = fclose(file) == 0;
    if (!close_ok) {
        report_error("WRITE", true);
        return;
    }

    upload.active = true;
    upload.expected_size = size;
    upload.expected_sha = sha;
    mbedtls_sha256_init(&upload.sha);
    upload.sha_initialized = true;
    if (mbedtls_sha256_starts_ret(&upload.sha, false) != 0) {
        report_error("SHA", true);
        return;
    }
    SERIAL_ECHOLNPAIR("FW_UPLOAD READY chunk=", maximum_chunk_size);
}

void write_chunk(const char *body) {
    if (!upload.active) {
        report_error("STATE");
        return;
    }
    uint32_t offset = 0;
    const char *encoded = value_after(body, 'D');
    if (!parse_u32(value_after(body, 'O'), offset) || offset != upload.received || !encoded) {
        report_error("OFFSET");
        return;
    }

    std::array<uint8_t, maximum_chunk_size> decoded {};
    const size_t encoded_size = strcspn(encoded, " ");
    size_t decoded_size = 0;
    if (encoded_size == 0 || encoded_size > 64
        || mbedtls_base64_decode(decoded.data(), decoded.size(), &decoded_size,
            reinterpret_cast<const unsigned char *>(encoded), encoded_size) != 0
        || decoded_size == 0 || decoded_size > upload.expected_size - upload.received) {
        report_error("CHUNK");
        return;
    }

    FILE *file = fopen(temporary_path, "r+b");
    bool write_ok = file && fseek(file, upload.received, SEEK_SET) == 0
        && fwrite(decoded.data(), 1, decoded_size, file) == decoded_size
        && fflush(file) == 0;
    if (file && fclose(file) != 0) write_ok = false;
    if (!write_ok) {
        report_error("WRITE", true);
        return;
    }
    if (mbedtls_sha256_update_ret(&upload.sha, decoded.data(), decoded_size) != 0) {
        report_error("SHA", true);
        return;
    }
    upload.received += decoded_size;
    SERIAL_ECHOLNPAIR("FW_UPLOAD OFFSET ", upload.received);
}

void finish_upload() {
    if (!upload.active || upload.received != upload.expected_size) {
        report_error("SIZE");
        return;
    }
    std::array<uint8_t, 32> actual_sha {};
    if (mbedtls_sha256_finish_ret(&upload.sha, actual_sha.data()) != 0 || actual_sha != upload.expected_sha) {
        report_error("HASH", true);
        return;
    }
    upload.sha_initialized = false;
    mbedtls_sha256_free(&upload.sha);
    upload.active = false;
    if (rename(temporary_path, firmware_path) != 0) {
        report_error("RENAME", true);
        return;
    }
    SERIAL_ECHOLN("FW_UPLOAD COMPLETE /usb/FWUPD.BBF");
}
} // namespace

/**
 * M998: binary-safe serial BBF upload to USB.
 * P0 S<size> H<sha256> starts; P1 O<offset> D<base64> writes; P2 finishes;
 * P3 aborts. Flash separately with M997 /usb/FWUPD.BBF.
 */
void PrusaGcodeSuite::M998() {
    const char *body = parser.string_arg ?: "";
    uint32_t phase = 0;
    if (!parse_u32(value_after(body, 'P'), phase)) {
        report_error("PHASE");
        return;
    }
    switch (phase) {
    case 0: begin_upload(body); break;
    case 1: write_chunk(body); break;
    case 2: finish_upload(); break;
    case 3:
        reset_state(true);
        SERIAL_ECHOLN("FW_UPLOAD ABORTED");
        break;
    default: report_error("PHASE"); break;
    }
}

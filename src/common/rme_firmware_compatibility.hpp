#pragma once
#include <cstdint>
#include <cstddef>

namespace buddy::rme_firmware_compatibility {

// BBF fields are authoritative; filenames and successful compilation are not.
constexpr bool supported(uint8_t major, uint8_t minor, uint8_t patch, uint8_t type, uint8_t version, uint8_t sub) {
    if (major != 6 || sub != (type == 1 && version == 3 ? 5 : 0)) {
        return false;
    }
    const bool core = (type == 7 || type == 8) && version == 1;
    const bool indx = type == 7 && version == 10;
    const bool xl = type == 3 && version == 1;
    const bool mini = type == 2 && version == 1;
    const bool mk = type == 1 && (version == 4 || version == 3);
    if (minor == 4 && patch == 2) {
        return xl || mini;
    }
    if (minor == 5 && (patch == 3 || patch == 7)) {
        return core || mk;
    }
    if (minor == 6 && patch <= 3) {
        return indx;
    }
    if (minor == 8 && patch == 1) {
        return core;
    }
    if (minor == 9 && patch <= 2) {
        return indx;
    }
    if (minor == 10 && patch >= 1 && patch <= 3) {
        return xl;
    }
    return false;
}

constexpr bool compatible(const uint8_t *header, size_t size, uint8_t type, uint8_t version, uint8_t sub) {
    if (size < 115) {
        return false;
    }
    const auto candidate_version = header[112] == 2 ? header[114] : header[112];
    return header[111] == type && candidate_version == version && header[113] == sub
        && supported(header[100], header[101], header[102], type, version, sub);
}
} // namespace buddy::rme_firmware_compatibility

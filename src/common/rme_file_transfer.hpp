#pragma once

#include <cstdint>
#include <string_view>

namespace rme_file_transfer {

inline constexpr uint32_t control_frame_offset = UINT32_C(0xfffffffe);
inline constexpr uint32_t abort_frame_offset = UINT32_C(0xffffffff);

constexpr bool plausible_binary_header(const uint32_t offset, const uint16_t payload_size,
    const uint32_t committed, const uint32_t expected_size, const uint16_t maximum_payload) {
    if (payload_size > maximum_payload) return false;
    if (offset == abort_frame_offset || offset == control_frame_offset) return true;
    return committed <= expected_size && offset == committed && payload_size <= expected_size - committed;
}

enum class BinaryFrameError : uint8_t {
    none,
    offset_mismatch,
    size_exceeded,
    crc_mismatch,
};

constexpr BinaryFrameError classify_binary_frame(const uint32_t offset, const uint32_t committed,
    const uint16_t payload_size, const uint32_t expected_size, const bool crc_matches) {
    if (offset != committed) return BinaryFrameError::offset_mismatch;
    if (committed > expected_size || payload_size > expected_size - committed) return BinaryFrameError::size_exceeded;
    if (!crc_matches) return BinaryFrameError::crc_mismatch;
    return BinaryFrameError::none;
}

constexpr const char *diagnostic_code(const BinaryFrameError error) {
    switch (error) {
    case BinaryFrameError::offset_mismatch: return "offset_mismatch";
    case BinaryFrameError::size_exceeded: return "size_exceeded";
    case BinaryFrameError::crc_mismatch: return "crc_mismatch";
    case BinaryFrameError::none: return nullptr;
    }
    return "invalid_frame";
}

constexpr bool consume_text_abort(const uint8_t byte, uint8_t &matched, const bool malformed_binary_frame) {
    constexpr std::string_view command = "@RME FILE ABORT";
    if (matched < command.size() && byte == static_cast<uint8_t>(command[matched])) {
        ++matched;
        return false;
    }
    if (matched == command.size() && malformed_binary_frame && (byte == '\r' || byte == '\n')) {
        matched = 0;
        return true;
    }
    matched = byte == static_cast<uint8_t>(command.front()) ? 1 : 0;
    return false;
}

} // namespace rme_file_transfer

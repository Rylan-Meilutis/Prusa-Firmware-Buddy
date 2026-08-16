#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <string_view>

namespace rme_file_transfer {

inline constexpr uint32_t control_frame_offset = UINT32_C(0xfffffffe);
inline constexpr uint32_t abort_frame_offset = UINT32_C(0xffffffff);

// Binary frames share the TinyUSB CDC receive FIFO with line traffic.  The
// complete advertised window must fit in that FIFO: hosts are explicitly
// allowed to transmit the window without waiting for an ACK.  Keep these
// values here (rather than duplicating literals in the service and docs) so a
// target build can prove that its advertised transport is physically viable.
inline constexpr size_t binary_header_size = 10;
inline constexpr size_t binary_payload_size = 512;
inline constexpr uint8_t binary_window_size = 3;
inline constexpr size_t binary_receive_backlog = binary_window_size * (binary_header_size + binary_payload_size);

// Text-bulk commands are pipelined. While one command is being committed to
// storage, TinyUSB must be able to retain every other command in that window.
// Keep these wire constants shared with the receiver-capacity assertion.
inline constexpr size_t bulk_payload_size = 384;
inline constexpr uint8_t bulk_window_size = 4;
inline constexpr size_t bulk_base64_size = ((bulk_payload_size + 2) / 3) * 4;
inline constexpr size_t bulk_command_overhead = sizeof("@RME FILE WRITE_BULK_CHUNK offset=4294967295 data=");
inline constexpr size_t bulk_receive_backlog = (bulk_window_size - 1) * (bulk_command_overhead + bulk_base64_size);

constexpr bool plausible_binary_header(const uint32_t offset, const uint16_t payload_size,
    const uint32_t committed, const uint32_t expected_size, const uint16_t maximum_payload) {
    if (payload_size > maximum_payload) {
        return false;
    }
    if (offset == abort_frame_offset) {
        return payload_size == 0;
    }
    if (offset == control_frame_offset) {
        return payload_size > 0;
    }
    return committed <= expected_size && offset == committed && payload_size <= expected_size - committed;
}

enum class HeaderScanResult : uint8_t {
    incomplete,
    invalid,
    ready,
};

// Feed the production rolling header window one byte at a time. Invalid
// lengths and offsets slide by one byte, so an abort/control header following
// arbitrary damage is still found without trusting a corrupt payload length.
constexpr HeaderScanResult scan_binary_header_byte(
    std::array<uint8_t, binary_header_size> &header, uint16_t &received,
    const uint8_t byte, const uint32_t committed, const uint32_t expected_size,
    const uint16_t maximum_payload, uint32_t &offset, uint16_t &payload_size,
    uint32_t &crc) {
    if (received < header.size()) {
        header[received++] = byte;
    }
    if (received != header.size()) {
        return HeaderScanResult::incomplete;
    }
    const auto le16 = [](const uint8_t *p) constexpr {
        return uint16_t(p[0]) | uint16_t(p[1]) << 8;
    };
    const auto le32 = [](const uint8_t *p) constexpr {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    };
    offset = le32(header.data());
    payload_size = le16(header.data() + 4);
    crc = le32(header.data() + 6);
    if (plausible_binary_header(offset, payload_size, committed, expected_size, maximum_payload)) {
        return HeaderScanResult::ready;
    }
    for (size_t index = 1; index < header.size(); ++index) {
        header[index - 1] = header[index];
    }
    received = header.size() - 1;
    return HeaderScanResult::invalid;
}

enum class BinaryFrameError : uint8_t {
    none,
    offset_mismatch,
    size_exceeded,
    crc_mismatch,
};

constexpr BinaryFrameError classify_binary_frame(const uint32_t offset, const uint32_t committed,
    const uint16_t payload_size, const uint32_t expected_size, const bool crc_matches) {
    if (offset != committed) {
        return BinaryFrameError::offset_mismatch;
    }
    if (committed > expected_size || payload_size > expected_size - committed) {
        return BinaryFrameError::size_exceeded;
    }
    if (!crc_matches) {
        return BinaryFrameError::crc_mismatch;
    }
    return BinaryFrameError::none;
}

constexpr const char *diagnostic_code(const BinaryFrameError error) {
    switch (error) {
    case BinaryFrameError::offset_mismatch:
        return "offset_mismatch";
    case BinaryFrameError::size_exceeded:
        return "size_exceeded";
    case BinaryFrameError::crc_mismatch:
        return "crc_mismatch";
    case BinaryFrameError::none:
        return nullptr;
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

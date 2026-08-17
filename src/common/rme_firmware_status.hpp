#pragma once

#include <array>
#include <cstdint>

namespace rme_firmware_status {

constexpr uint32_t verified_metadata_magic = UINT32_C(0x524d4656); // "RMFV"
constexpr uint16_t verified_metadata_version = 1;
constexpr uint32_t query_hash_timeout_ms = 15'000;
constexpr uint32_t query_busy_interval_ms = 1'000;

struct VerifiedMetadata {
    uint32_t magic = verified_metadata_magic;
    uint16_t version = verified_metadata_version;
    uint16_t reserved = 0;
    uint32_t size = 0;
    std::array<uint8_t, 32> sha256 {};
};

constexpr bool valid(const VerifiedMetadata &metadata, const uint32_t candidate_size) {
    return metadata.magic == verified_metadata_magic
        && metadata.version == verified_metadata_version
        && metadata.reserved == 0
        && metadata.size != 0
        && metadata.size == candidate_size;
}

constexpr bool elapsed(const uint32_t now, const uint32_t then, const uint32_t interval) {
    return static_cast<uint32_t>(now - then) >= interval;
}

constexpr bool complete_read_valid(const uint32_t processed, const uint32_t expected,
    const bool eof, const bool read_error) {
    return eof && !read_error && processed == expected;
}

} // namespace rme_firmware_status

#pragma once

#include <array>
#include <span>
#include <optional>
#include <cstdint>
#include <utility>

namespace nfcv {
constexpr size_t UID_SIZE = 8;
using UID = std::array<std::byte, UID_SIZE>;
using UIDView = std::span<std::byte, UID_SIZE>;
using UIDConstView = std::span<const std::byte, UID_SIZE>;
using BlockID = uint8_t;

struct TagInfo {
    using DSFID = uint8_t;
    using AFI = uint8_t;
    struct MemorySize {
        uint8_t block_size;
        uint8_t block_count;
    };
    using ICRef = uint8_t;

    std::optional<DSFID> dsfid;
    std::optional<AFI> afi;
    std::optional<MemorySize> mem_size;
    std::optional<ICRef> ic_ref;
};
} // namespace nfcv

#pragma once

#include <array>
#include <span>

namespace nfcv {
constexpr std::size_t UID_SIZE = 8;
using UID = std::array<std::byte, UID_SIZE>;
using UIDView = std::span<std::byte, UID_SIZE>;
using UIDConstView = std::span<const std::byte, UID_SIZE>;
} // namespace nfcv

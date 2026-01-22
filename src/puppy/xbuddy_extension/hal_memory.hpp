/// @file
#pragma once

#include <cstddef>
#include <span>

namespace hal::memory {

/// Memory address region dedicated to peripherals control
extern const std::span<std::byte> peripheral_address_region;

} // namespace hal::memory

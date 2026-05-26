#pragma once

#include <cstdint>

namespace leds {

enum class ExternalLightMode : uint8_t {
    off = 0,
    mirror_chamber = 1,
    active_and_printing = 2,
    printing_only = 3,
    idle_only = 4,
    awake = 5,
    _cnt = 6,
};

} // namespace leds

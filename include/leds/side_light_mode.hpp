#pragma once

#include <cstdint>

namespace leds {

enum class SideLightMode : uint8_t {
    off = 0,
    active_and_printing = 1,
    idle_only = 2,
    awake = 3,
    _cnt = 4,
};

} // namespace leds

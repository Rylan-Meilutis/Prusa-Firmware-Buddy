#pragma once

#include <cstdint>

namespace leds {

enum class LightState : uint8_t {
    deep_idle = 0,
    idle = 1,
    active = 2,
    printing = 3,
};

constexpr uint8_t light_state_bit(LightState state) {
    return static_cast<uint8_t>(1U << static_cast<uint8_t>(state));
}

} // namespace leds

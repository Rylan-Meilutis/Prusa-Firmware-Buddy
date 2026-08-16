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

constexpr uint8_t light_state_shift(LightState state) {
    return static_cast<uint8_t>(8U * static_cast<uint8_t>(state));
}

constexpr uint8_t minimum_screen_brightness(LightState state) {
    return state == LightState::active || state == LightState::printing ? 15 : 0;
}

constexpr uint8_t clamp_screen_brightness(LightState state, uint8_t value) {
    const uint8_t minimum = minimum_screen_brightness(state);
    return value < minimum ? minimum : (value > 100 ? 100 : value);
}

/// True while a bounded startup-activity window is open. Unsigned subtraction
/// keeps the comparison correct when the millisecond counter wraps.
constexpr bool startup_activity_within_window(
    uint32_t started_ms, uint32_t now_ms, uint32_t duration_ms) {
    return now_ms - started_ms < duration_ms;
}

} // namespace leds

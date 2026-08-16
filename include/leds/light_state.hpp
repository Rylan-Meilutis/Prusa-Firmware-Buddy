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

constexpr uint32_t sanitize_screen_brightness_by_state(uint32_t values) {
    constexpr LightState states[] = {
        LightState::deep_idle,
        LightState::idle,
        LightState::active,
        LightState::printing,
    };
    uint32_t sanitized = 0;
    for (const auto state : states) {
        const uint8_t shift = light_state_shift(state);
        const uint8_t value = (values >> shift) & 0xff;
        sanitized |= static_cast<uint32_t>(clamp_screen_brightness(state, value)) << shift;
    }
    return sanitized;
}

/// True while a bounded startup-activity window is open. Unsigned subtraction
/// keeps the comparison correct when the millisecond counter wraps.
constexpr bool startup_activity_within_window(
    uint32_t started_ms, uint32_t now_ms, uint32_t duration_ms) {
    return now_ms - started_ms < duration_ms;
}

} // namespace leds

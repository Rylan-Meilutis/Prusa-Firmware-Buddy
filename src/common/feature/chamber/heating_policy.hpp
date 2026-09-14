#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace buddy::chamber_heating {

constexpr float hysteresis_c = 1.0f;

constexpr bool should_assist(std::optional<float> current, std::optional<float> target, bool printing, bool blocking_heat_wait) {
    return current.has_value() && target.has_value() && *target > 0
        && *current < *target - hysteresis_c
        && (!printing || blocking_heat_wait);
}

template <class T>
constexpr T assisted_output(T previous, T minimum) {
    return std::max(previous, minimum);
}

} // namespace buddy::chamber_heating

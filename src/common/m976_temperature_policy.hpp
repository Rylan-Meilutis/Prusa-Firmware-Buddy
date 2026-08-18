#pragma once

#include <cstdint>

namespace buddy::m976_temperature_policy {

inline constexpr int16_t probe_margin_celsius = 5;

constexpr int16_t probe_temperature(const int16_t extrusion_minimum) {
    return extrusion_minimum + probe_margin_celsius;
}

// M976 restores its caller's target immediately. A following M109 owns any
// required cooldown so the serial command can complete without a host timeout.
inline constexpr bool wait_for_restored_target = false;

} // namespace buddy::m976_temperature_policy

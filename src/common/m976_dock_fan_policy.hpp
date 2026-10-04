#pragma once

#include <cstddef>
#include <cstdint>

namespace buddy::m976_dock_fan_policy {

// Parked INDX tools have no live temperature telemetry. Use the hottest
// requested calibration temperature as a conservative batch cooling budget.
// Keep a usable starting duty (50% at 170 C), ramp to full at 300 C.
// Never reduce cooling already requested by the user/print profile.
constexpr uint16_t pwm(size_t uncached_tools, int hottest_temperature, uint16_t previous) {
    if (uncached_tools <= 1) {
        return previous;
    }
    const int temperature = hottest_temperature < 170 ? 170 : (hottest_temperature > 300 ? 300 : hottest_temperature);
    const auto requested = static_cast<uint16_t>(128 + (temperature - 170) * 127 / 130);
    return requested > previous ? requested : previous;
}

} // namespace buddy::m976_dock_fan_policy

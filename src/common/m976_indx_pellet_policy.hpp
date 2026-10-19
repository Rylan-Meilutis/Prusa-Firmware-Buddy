#pragma once

#include <cstdint>
#include <string_view>

namespace buddy::m976_indx_pellet_policy {

inline constexpr uint8_t cycles_per_ejection = 5;
inline constexpr uint16_t cooling_delay_ms = 15000;
inline constexpr uint8_t main_wiper_passes = 2;
inline constexpr float maximum_pending_filament_mm = 12.0f; // Five rigid excitation cycles.

struct Budget {
    uint8_t cycles = 0;
    float filament_mm = 0;

    bool would_overfill(float next_mm) const {
        return next_mm < 0 || !(next_mm <= maximum_pending_filament_mm)
            || filament_mm + next_mm > maximum_pending_filament_mm + 0.0001f;
    }
    bool record(float mm) {
        filament_mm += mm;
        return ++cycles >= cycles_per_ejection || filament_mm >= maximum_pending_filament_mm - 0.0001f;
    }
};

// Every material gets longer than the previous high-temperature dwell.
// This is a timing policy, not a measurement of pellet solidification.
constexpr uint16_t cooling_delay_for(std::string_view, uint16_t) {
    return cooling_delay_ms;
}

constexpr bool record_cycle_and_should_eject(uint8_t &cycles_since_ejection) {
    ++cycles_since_ejection;
    return cycles_since_ejection >= cycles_per_ejection;
}

constexpr bool final_ejection_needed(const uint8_t cycles_since_ejection) {
    return cycles_since_ejection != 0;
}

} // namespace buddy::m976_indx_pellet_policy

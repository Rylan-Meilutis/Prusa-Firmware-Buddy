#pragma once

#include <cstdint>
#include <string_view>

namespace buddy::m976_indx_pellet_policy {

inline constexpr uint8_t cycles_per_ejection = 5;
inline constexpr uint16_t cooling_delay_ms = 4000;
inline constexpr uint16_t extended_cooling_delay_ms = 12000;

// Material families cover cooler custom profiles too; the temperature fallback
// covers unrecognized high-temperature materials. This is a timing policy,
// not a measurement of pellet solidification.
constexpr uint16_t cooling_delay_for(std::string_view material, uint16_t nozzle_temperature) {
    return material.starts_with("PET") || material.starts_with("PCTG")
            || material.starts_with("ASA") || material.starts_with("ABS")
            || material.starts_with("PC") || material.starts_with("PA")
            || material.starts_with("PPA") || material.starts_with("HIPS")
            || nozzle_temperature >= 230
        ? extended_cooling_delay_ms
        : cooling_delay_ms;
}

constexpr bool record_cycle_and_should_eject(uint8_t &cycles_since_ejection) {
    ++cycles_since_ejection;
    return cycles_since_ejection >= cycles_per_ejection;
}

constexpr bool final_ejection_needed(const uint8_t cycles_since_ejection) {
    return cycles_since_ejection != 0;
}

} // namespace buddy::m976_indx_pellet_policy

#pragma once

#include <cstdint>

namespace buddy::m976_indx_pellet_policy {

inline constexpr uint8_t cycles_per_ejection = 5;

constexpr bool record_cycle_and_should_eject(uint8_t &cycles_since_ejection) {
    ++cycles_since_ejection;
    return cycles_since_ejection >= cycles_per_ejection;
}

constexpr bool final_ejection_needed(const uint8_t cycles_since_ejection) {
    return cycles_since_ejection != 0;
}

} // namespace buddy::m976_indx_pellet_policy

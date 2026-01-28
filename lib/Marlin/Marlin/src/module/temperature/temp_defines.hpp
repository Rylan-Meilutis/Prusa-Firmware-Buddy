/// @file
#pragma once

#include <inc/MarlinConfig.h>

#ifndef SOFT_PWM_SCALE
    #define SOFT_PWM_SCALE 0
#endif

#if HOTENDS <= 1
    #define HOTEND_INDEX 0
    #define E_NAME
#else
    #define HOTEND_INDEX e
    #define E_NAME       e
#endif

// Identifiers for other heaters
enum heater_ind_t : int8_t {
    INDEX_NONE = -4,
    H_REDUNDANT,
    H_BOARD,
    H_BED,
    H_NOZZLE_FIRST,
    H_NOZZLE_LAST = H_NOZZLE_FIRST + HOTENDS - 1,
    H_HEATBREAK_FIRST,
    H_HEATBREAK_LAST = H_HEATBREAK_FIRST + HOTENDS - 1,
};
static_assert(H_NOZZLE_FIRST == 0); // lots of places in are indexed by this, and assumes H_E0 is zero

/// A bold assumption used by steady_state_hotend and temp regulator
static constexpr float ambient_temp = 21.0f;

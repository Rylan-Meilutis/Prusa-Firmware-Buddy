/// @file
#pragma once

#include <cstdint>
#include <inc/MarlinConfigPre.h>

// Temporary declares until everything is moved from temperature.cpp to hotends

#if ENABLED(HW_PWM_HEATERS)
static constexpr uint8_t soft_pwm_bit_shift = 0;
#else
static constexpr uint8_t soft_pwm_bit_shift = 1;
#endif

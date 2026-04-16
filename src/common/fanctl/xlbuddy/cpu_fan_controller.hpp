#pragma once

#include <option/has_cpu_fan.h>

#if HAS_CPU_FAN()

    #include <algorithm>
    #include <cmath>
    #include <cstdint>
    #include <CFanCtlCommonConsts.hpp>

namespace cpu_fan_controller {

// Hysteretic on/off thresholds (°C). The LDO-D3007D04Y05X75FX is rated
// 3.5-7 V with a 3.5 V starting voltage
inline constexpr float temp_off = 55.0f;
inline constexpr float temp_on = 65.0f;
inline constexpr float temp_full = 80.0f;

static_assert(temp_off < temp_on && temp_on < temp_full, "temperature thresholds are invalid");

/// Pure hysteretic fan policy. Given the two temperature sensors and the
/// currently applied PWM, returns the PWM to set. Inside the hysteresis band
/// (temp_off..temp_on) the fan keeps its previous on/off state, so
/// `current_pwm` is returned unchanged there. Kept header-inline so it can be
/// unit-tested without the Fans hardware singleton; update() feeds it the
/// live PWM.
constexpr uint16_t compute_pwm(float mcu_temp_c, float sandwich_temp_c, uint16_t current_pwm) {
    const float temp = std::max(mcu_temp_c, sandwich_temp_c);

    // Fail-safe: default ON when sensor data is unreliable.
    if (!std::isfinite(temp)) {
        return static_cast<uint16_t>(255.f * FANCTLCPU_PWM_MAX / 100.f);
    }

    // Preserve current state inside the hysteresis band.
    if (temp >= temp_full) {
        return static_cast<uint16_t>(FANCTLCPU_PWM_MAX * 255.f / 100.f);
    } else if (temp >= temp_on) {
        return static_cast<uint16_t>(FANCTLCPU_PWM_THR * 255.f / 100.f) + static_cast<uint16_t>((temp - temp_on) * (FANCTLCPU_PWM_MAX - FANCTLCPU_PWM_THR) * 255.f / (temp_full - temp_on) / 100.f);
    } else if (temp <= temp_off) {
        return 0;
    }
    return current_pwm;
}

/// Update CPU fan speed based on MCU and sandwich-board temperatures.
/// Runs a hysteretic on/off policy. The LDO-D3007D04Y05A00FX is rated
/// down to a 3.5 V starting voltage so it could in principle run at
/// reduced duty, but the on/off policy is intentionally simple --
/// thermals on XLS don't need fine-grained speed control. Called from
/// the main idle loop on the XLS variant; plain XL never calls this
/// and the fan stays at 0.
void update(float mcu_temp_c, float sandwich_temp_c);

} // namespace cpu_fan_controller

#endif // HAS_CPU_FAN()

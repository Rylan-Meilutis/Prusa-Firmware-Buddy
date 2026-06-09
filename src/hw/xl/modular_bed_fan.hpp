/**
 * @file
 * @brief MB MCU temperature driven control policy for the XLS Modular Bed cooling fan
 */

#pragma once

#include <algorithm>
#include <cstdint>

namespace buddy {

/// Off below temp_on_c, then a linear ramp min_pwm..max_pwm reached at
/// temp_full_c. The on/off threshold has hysteresis so the fan does not chatter
/// when the integer MCU temperature dithers around temp_on_c.
class ModularBedFanControl {
public:
    static constexpr int temp_on_c = 50; ///< [°C] at/above: fan runs
    static constexpr int temp_off_c = 48; ///< [°C] below: a running fan stops (hysteresis)
    static constexpr int temp_full_c = 65; ///< [°C] at/above: run at max_pwm
    static constexpr uint8_t min_pwm = 76; ///< ~30 % of 255; the fan may not spin below this
    static constexpr uint8_t max_pwm = 153; ///< 60 % of 255

    /// Fan duty (0-255) for the current MB MCU temperature [°C].
    [[nodiscard]] uint8_t update(int mcu_temperature_c) {
        if (running_) {
            running_ = mcu_temperature_c >= temp_off_c;
        } else {
            running_ = mcu_temperature_c >= temp_on_c;
        }
        if (!running_) {
            return 0;
        }
        if (mcu_temperature_c >= temp_full_c) {
            return max_pwm;
        }
        // Clamp the ramp's lower end so it yields min_pwm in the still-running
        // hysteresis band (temp_off_c..temp_on_c) instead of dropping below it.
        const int t = std::max(mcu_temperature_c, temp_on_c);
        return static_cast<uint8_t>(min_pwm + (t - temp_on_c) * (max_pwm - min_pwm) / (temp_full_c - temp_on_c));
    }

private:
    bool running_ = false;
};

} // namespace buddy

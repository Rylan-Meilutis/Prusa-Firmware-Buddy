/// @file
#include "heater_watch.hpp"

#include <module/temperature.h>
#include <bsod.h>

void HeaterWatchBase::reset(const Config &config, float current_temp, int16_t target_temp) {
    if ((target_temp > 0)
        // Invert the comparison logic if check_for_cooling_instead
        // Note: current_temp is float, so equality case probably doesn't matter
        && ((target_temp - current_temp) > config.min_temp_diff) ^ config.watch_cooling_instead //
    ) {
        target = static_cast<int16_t>(current_temp) + config.temp_increase;
        next_ms = millis() + config.period_s * 1000UL;

    } else {
        next_ms = 0;
    }
}

void HeaterWatchBase::check(const Config &config, float current_temp, int16_t target_temp) {
    if (!next_ms || !ELAPSED(millis(), next_ms)) {
        return;
    }

    // Invert the comparison logic if check_for_cooling_instead
    // Note: current_temp is float, so equality case probably doesn't matter
    if ((current_temp < target) ^ config.watch_cooling_instead) {
        fatal_error(config.error_code);
    }

    reset(config, current_temp, target_temp);
}

/// @file
#include "heater_watch.hpp"

#include <module/temperature.h>
#include <bsod.h>

void HeaterWatchBase::arm(int16_t target_temp) {
    if (target_temp <= 0) {
        state_ = State::disarmed;
        return;
    }
    target_temp_ = target_temp;
    state_ = State::pending;
}

void HeaterWatchBase::update(const Config &config, float current_temp) {
    switch (state_) {
    case State::disarmed:
        return;

    case State::watching:
        if (!ELAPSED(millis(), next_check_ms_)) {
            return;
        }
        if ((current_temp < baseline_threshold_) ^ config.watch_cooling_instead) {
            fatal_error(config.error_code);
        }
        // Period ended successfully — re-evaluate engage condition for the next one
        state_ = State::pending;
        [[fallthrough]];

    case State::pending:
        if (!(((target_temp_ - current_temp) > config.min_temp_diff) ^ config.watch_cooling_instead)) {
            state_ = State::disarmed;
            return;
        }
        baseline_threshold_ = static_cast<int16_t>(current_temp) + config.temp_increase;
        next_check_ms_ = millis() + config.period_s * 1000UL;
        state_ = State::watching;
        return;
    }
}

/// @file
#pragma once

#include <module/temperature/temp_defines.hpp>

class HeatbreakRegulator {

public:
    struct Args {
        float current_temp;
        int16_t target_temp;

        float current_hotend_temp;
    };

    /// Steps the regulator.
    /// @returns the new PWM of the heatbreak fan
    float step(const Args &args);

private:
    PID_t work_pid;
    float temp_iState = 0, temp_dState = 0;
    bool pid_reset = true;
    int fan_kick_counter = 0;
};

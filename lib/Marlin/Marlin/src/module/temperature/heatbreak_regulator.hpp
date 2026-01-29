/// @file
#pragma once

#include <module/temperature/temp_defines.hpp>

class HeatbreakRegulator {

public:
    /// Steps the regulator.
    /// @returns the new PWM of the heatbreak fan
    float step();

private:
    PID_t work_pid;
    float temp_iState = 0, temp_dState = 0;
    bool pid_reset = true;
    int fan_kick_counter = 0;
};

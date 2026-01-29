/// @file
#pragma once

#include <cstdint>

#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>
#include <module/temperature/hotend_regulator/hotend_regulator.hpp>

class StandardHotendRegulator {

public:
    HotendRegulatorResult get_pid_output_hotend(const HotendRegulatorArgs &args);

private:
    hotend_pid_t work_pid;
    float temp_iState = 0;
    float temp_dState = 0;
    bool pid_reset = false;
};

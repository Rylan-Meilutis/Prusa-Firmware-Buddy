/// @file
#pragma once

#include <cstdint>
#include <inc/MarlinConfig.h>

class StandardHotendRegulator {

public:
    float get_pid_output_hotend(
        const uint8_t e);
};

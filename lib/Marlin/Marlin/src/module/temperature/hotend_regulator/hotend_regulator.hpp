/// @file
#pragma once

#include <cstdint>

struct HotendRegulatorArgs {
    /// Hotend index
    uint8_t hotend_index;
};

struct HotendRegulatorResult {
    float pid_output;
    float feed_forward;
};

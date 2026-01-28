/// @file
#pragma once

#include <cstdint>
#include <inc/MarlinConfig.h>

class ModelBasedHotendRegulator {

public:
    float get_pid_output_hotend(
#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
        float &feed_forward,
#endif
        const uint8_t e);

private:
    float get_model_output_hotend(float &last_target, float &expected, const uint8_t e);
};

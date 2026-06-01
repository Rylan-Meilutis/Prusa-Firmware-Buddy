#pragma once
#include <contactless_offset/tool_sensor.hpp>
#include "clo_config.hpp"
#include <cstdint>
#include <expected>

namespace tool_offset {

struct ToolOffset {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Measure the current tool's XYZ offset relative to the sensor reference position.
// Performs homing, Z probing, and two-pass XY scanning internally.
std::expected<ToolOffset, const char *> measure_current_tool_offset(
    const ProbingConfig &config,
    Sensor &sensor,
    const ToolOffset &actual_offset);

// Block until the INDX loadcell stream produces a fresh sample.
// if no sample arrives within the timeout, the function performs a loadcell rearm (toggle off -> on)
// the first samples can be stale or missing entirely
// Returns true on success, false if the stream never resumed.
bool wait_for_loadcell_alive(uint32_t per_attempt_timeout_us = 500'000, uint8_t retries = 3);

} // namespace tool_offset

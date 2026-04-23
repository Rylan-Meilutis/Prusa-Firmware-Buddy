#pragma once
#include <contactless_offset/tool_sensor.hpp>
#include "clo_config.hpp"
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

} // namespace tool_offset

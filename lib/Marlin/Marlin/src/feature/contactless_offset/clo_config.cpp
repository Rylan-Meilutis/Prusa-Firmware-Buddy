#include "clo_config.hpp"

#include <option/has_indx_head.h>
#include <option/tool_offset_sensor_geometry.h>

namespace {
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
constexpr float y_shift_z_probe_offset_from_sensor = -3.2f; // See BFW-8747 geometric shift to move the probe point out of the coil area
// One physical coil, described once per axis: the Y sweep is longer than the X one.
constexpr tool_offset::CoilAxis coil_x {
    .position = { { { tool_offset::default_sensor_position.x, tool_offset::default_sensor_position.y, 0.f } } },
    .channel = tool_offset::SensorChannel::ch1,
    .sensing_distance = 6.f,
};
constexpr tool_offset::CoilAxis coil_y {
    .position = coil_x.position,
    .channel = coil_x.channel,
    .sensing_distance = 12.f,
};
#elif PRINTER_IS_PRUSA_XL()
constexpr tool_offset::CoilAxis coil_x {
    .position = { { { 5.f, -5.f, 0.f } } },
    .channel = tool_offset::SensorChannel::ch1,
    .sensing_distance = 12.f,
};
constexpr tool_offset::CoilAxis coil_y {
    .position = { { { -6.f, 6.f, 0.f } } },
    .channel = tool_offset::SensorChannel::ch0,
    .sensing_distance = 12.f,
};
#else
    #error "sensor parameters not defined for this printer"
#endif
} // namespace

tool_offset::ProbingConfig tool_offset::get_default_probing_config() {
    ProbingConfig config {
        .coil_x = coil_x,
        .coil_y = coil_y,
        .safe_z_height = 4.f, // mm
        .travel_z_height = 10.f,
        .sensing_z = 0.2f,
        .sensing_speed_slow = 20.f,
        .sensing_speed_fast = 30.f,
        .sweep_rest_time = 0.35f,
        .max_safe_temp = 180.f,
        .symmetry_trim_fraction = 0.5f,
    };
#if TOOL_OFFSET_SENSOR_GEOMETRY_IS_SINGLE_COIL()
    config.y_shift_z_probe_offset_from_sensor = y_shift_z_probe_offset_from_sensor;
#endif
    return config;
}

static_assert(coil_x.position.x - coil_x.sensing_distance / 2.0f >= X_MIN_POS, "X-sweep coil exceeds printer's physical limits");
static_assert(coil_x.position.x + coil_x.sensing_distance / 2.0f <= X_MAX_POS, "X-sweep coil exceeds printer's physical limits");
static_assert(coil_x.position.y >= Y_MIN_POS && coil_x.position.y <= Y_MAX_POS, "X-sweep coil is out of Y reach");
static_assert(coil_y.position.y - coil_y.sensing_distance / 2.0f >= Y_MIN_POS, "Y-sweep coil exceeds printer's physical limits");
static_assert(coil_y.position.y + coil_y.sensing_distance / 2.0f <= Y_MAX_POS, "Y-sweep coil exceeds printer's physical limits");
static_assert(coil_y.position.x >= X_MIN_POS && coil_y.position.x <= X_MAX_POS, "Y-sweep coil is out of X reach");

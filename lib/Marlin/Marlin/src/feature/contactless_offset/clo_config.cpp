#include "clo_config.hpp"

namespace {
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
constexpr float y_shift_z_probe_offset_from_sensor = -3.2f; // See BFW-8747 geometric shift to move the probe point out of the coil area
constexpr float sensing_distance_x = 6.f;
constexpr float sensing_distance_y = 12.f;
#elif PRINTER_IS_PRUSA_XL()
constexpr float sensing_distance_x = 10.f;
constexpr float sensing_distance_y = 10.f;
constexpr float y_shift_z_probe_offset_from_sensor = 0.f;
#else
    #error "sensor parameters not defined for this printer"
#endif
} // namespace

tool_offset::ProbingConfig tool_offset::get_default_probing_config() {
    return ProbingConfig {
        .sensor_position = { { { default_sensor_position.x, default_sensor_position.y, 0.f } } }, // mm
        .safe_z_height = 4.f,
        .travel_z_height = 10.f,
        .sensing_z = 0.2f,
        .sensing_distance_x = sensing_distance_x,
        .sensing_distance_y = sensing_distance_y,
        .sensing_speed_slow = 20.f,
        .sensing_speed_fast = 30.f,
        .sweep_rest_time = 0.35f,
        .max_safe_temp = 180.f,
        .symmetry_trim_fraction = 0.5f,
        .y_shift_z_probe_offset_from_sensor = y_shift_z_probe_offset_from_sensor,
    };
}

static_assert(tool_offset::default_sensor_position.x + sensing_distance_x / 2.0f <= X_MAX_POS, "Sensor position definition exceeds printer's physical limits");
static_assert(tool_offset::default_sensor_position.y + sensing_distance_y / 2.0f <= Y_MAX_POS, "Sensor position definition exceeds printer's physical limits");
static_assert(tool_offset::default_sensor_position.x - sensing_distance_x / 2.0f >= X_MIN_POS, "Sensor position definition exceeds printer's physical limits");
static_assert(tool_offset::default_sensor_position.y - sensing_distance_y / 2.0f >= Y_MIN_POS, "Sensor position definition exceeds printer's physical limits");

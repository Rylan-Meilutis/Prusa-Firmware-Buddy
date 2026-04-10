#include <Marlin.h>
#include <gcode/gcode.h>
#include <feature/contactless_offset/contactless_offset.hpp>
#include <module/motion.h>
#include <module/planner.h>
#include <feature/pressure_advance/pressure_advance_config.hpp>
#include <utils/variant_utils.hpp>
#include <option/has_toolchanger.h>
#include <module/tool_change.h>
#include <module/prusa/toolchanger.h>

/** \addtogroup G-Codes
 * @{
 */

/**
 * ### G426: Measure tool offset using contactless sensor
 *
 * Uses the induction sensor to measure the current tool's offset relative to a
 * reference position.
 *
 * #### Usage
 *
 *     G426 [F<speed>] [R<speed2>] [Z<height>] [D<diameter>] [X<pos_x>] [Y<pos_y>]
 *
 * #### Parameters
 *
 * - `F` - First speed (v1) in mm/s (default: 7)
 * - `R` - Second speed (v2) in mm/s (default: 15)
 * - `Z` - Sensing height in mm above the sensor (default: from config)
 * - `D` - Scan diameter in mm (default: from config)
 * - `X` - position of the sensor in X axis (default: from config)
 * - `Y` - position of the sensor in Y axis (default: from config)
 */
void GcodeSuite::G426() {
    TEMPORARY_AUTO_REPORT_OFF(suspend_auto_report);
    SERIAL_ECHOLN("G426 contactless offset measurement");

    auto config = tool_offset::get_default_probing_config();

    if (parser.seenval('F')) {
        config.sensing_speed_slow = parser.value_float();
    }
    if (parser.seenval('R')) {
        config.sensing_speed_fast = parser.value_float();
    }
    if (parser.seenval('Z')) {
        config.sensing_z = parser.value_float();
    }
    if (parser.seenval('D')) {
        config.sensing_diameter = parser.value_float();
    }
    if (parser.seenval('X')) {
        config.sensor_position.x = parser.value_float();
    }
    if (parser.seenval('Y')) {
        config.sensor_position.y = parser.value_float();
    }

    const auto selected_tool = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::currently_selected());
    if (!selected_tool.has_value()) {
        SERIAL_ECHOLNPAIR("G426 failed: no tool selected");
        return;
    }

    // Reset planner state
    planner.synchronize();
    planner.reset_position();

    // Disable PA to reduce filter delay during probe analysis
    pressure_advance::PressureAdvanceDisabler pa_disabler;

    // Perform the measurement for picked tool
    auto sensor = tool_offset::get_default_sensor();
    tool_offset::ToolOffset actual_ho = {
        .x = hotend_offset[selected_tool.value()].x,
        .y = hotend_offset[selected_tool.value()].y,
        .z = hotend_offset[selected_tool.value()].z,
    };
    // Zero hotend offset and currently applied offset
    reset_hotend_offset(selected_tool.value());
    hotend_currently_applied_offset = xyz_pos_t {};

    auto result = tool_offset::measure_current_tool_offset(config, *sensor, actual_ho);
    if (!result.has_value()) {
        SERIAL_ECHOLNPAIR("G426 failed: ", result.error());
        return;
    }

    // Store newly measured offsets only for XY, keep actual for Z
    hotend_offset[selected_tool.value()].x = result->x;
    hotend_offset[selected_tool.value()].y = result->y;
    prusa_toolchanger.save_tool_offset(selected_tool.value());
    hotend_currently_applied_offset = xyz_pos_t { result->x, result->y, actual_ho.z };

    SERIAL_ECHOPGM("X offset: ");
    SERIAL_ECHO_F(result->x, 4);
    SERIAL_ECHOLNPGM(" mm");
    SERIAL_ECHOPGM("Y offset: ");
    SERIAL_ECHO_F(result->y, 4);
    SERIAL_ECHOLNPGM(" mm");
    SERIAL_ECHOPGM("Z offset: ");
    SERIAL_ECHO_F(result->z, 4);
    SERIAL_ECHOLNPGM(" mm");
}

/** @}*/

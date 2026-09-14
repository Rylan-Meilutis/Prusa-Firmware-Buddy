/// @file
/// @brief G427: Full tool offset calibration (Z-offset via probing + XY-offset with tool_offset board)

#include "PrusaGcodeSuite.hpp"
#include <feature/tool_offset_calibration/tool_offset_calibration.hpp>
#include <g427_tool_selection.hpp>
#include <common/serial_printing.hpp>
#include <option/has_side_leds.h>
#if HAS_SIDE_LEDS()
    #include <leds/side_strip_handler.hpp>
#endif

/** \addtogroup G-Codes
 * @{
 */

/**
 *### G427: Tool offset calibration
 *
 * Runs the full tool offset calibration sequence for all mapped tools:
 *  1. Determine which physical tools are needed from tool mapping + spool join
 *  2. For each tool calibrate XYZ
 *  3. Set results to runtime variables and save to EEPROM
 *
 *#### Usage
 *
 *    G427 [R | P]
 *    G427 T0,7 [R | P]
 *
 *#### Parameters
 *
 * - `R` - millimeters of random jitter on X & Y axis during z_probing each tool <0;255>
 * - `P` - number of Z probe repetitions per point to average (default 1) <1;255>
 * - `T` - comma-separated physical tools to calibrate. When supplied, this is
 *   authoritative and avoids relying on file metadata during serial printing.
 */
namespace PrusaGcodeSuite {

void G427() {
#if HAS_SIDE_LEDS()
    leds::ScopedActiveLightHold active_light_hold;
#endif
    GCodeParser2 parser;
    if (!parser.parse_marlin_command()) {
        return;
    }

    uint8_t r_param = 0;
    (void)parser.store_option_if_present('R', r_param);

    uint8_t p_param = 1;
    (void)parser.store_option_if_present('P', p_param);
    p_param = std::max<uint8_t>(p_param, 1);

    const auto tool_selection = g427_tool_selection::parse(parser.gcode(), PhysicalToolIndex::count);
    if (tool_selection.present && !tool_selection.mask.has_value()) {
        SERIAL_ERROR_MSG("G427 invalid physical tool list");
        return;
    }

    SerialPrinting::notify_workflow("indx_tool_offset_calibration", "calibrating", "Tool offset calibration", 0);
    const auto progress_cb = [](const tool_offset_calibration::ProgressReport &progress) {
        const int percent = progress.total_steps
            ? (static_cast<int>(progress.step - 1) * 100) / progress.total_steps
            : 0;
        SerialPrinting::notify_workflow("indx_tool_offset_calibration", "calibrating", "Calibrating tool offsets", percent);
        return true;
    };
    const bool success = tool_offset_calibration::run(r_param, p_param, tool_offset_calibration::Context::Print, progress_cb, tool_selection.mask);
    SerialPrinting::notify_workflow("indx_tool_offset_calibration", success ? "success" : "failed",
        success ? "Tool offset calibration complete" : "Tool offset calibration failed", success ? 100 : -1);
}

} // namespace PrusaGcodeSuite

/** @}*/

/// @file
/// @brief M1985: Tool offsets calibration wizard

#include <option/has_tool_offset_sensor.h>

#if HAS_TOOL_OFFSET_SENSOR()

    #include "PrusaGcodeSuite.hpp"
    #include <feature/tool_offset_wizard/tool_offset_wizard.hpp>
    #include <option/has_side_leds.h>
    #if HAS_SIDE_LEDS()
        #include <leds/side_strip_handler.hpp>
    #endif

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1985: Tool offsets calibration
 *
 * Runs the selftest wizard that calibrates the XY offset of every physical tool against the
 * tool offset sensor and updates the stored sensor position. Z offsets are left untouched (they
 * are populated by G427 at print start).
 *
 *#### Usage
 *
 *    M1985
 */
void PrusaGcodeSuite::M1985() {
    #if HAS_SIDE_LEDS()
    leds::ScopedActiveLightHold active_light_hold;
    #endif
    tool_offset_wizard::run();
}

/** @}*/

#endif

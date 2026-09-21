/**
 * @file
 */
#include "src/module/prusa/toolchanger.h"
#include "../gcode.h"
#include "PrusaGcodeSuite.hpp"
#include <module/tool_change.h>
#include <option/has_indx.h>
#if HAS_INDX()
    #include <mapi/parking.hpp>
    #include <module/motion.h>
    #include <nozzle_cleaner.hpp>
    #include <algorithm>
#endif

/** \addtogroup G-Codes
 * @{
 */

/**
 *### P0: Park extruder (tool)
 *
 * Internal GCode
 *
 * Only XL & printers with INDX tools
 *
 *#### Usage
 *
 *    P0 [ S | L | D ]
 *
 *#### Parameters
 *
 * - `S` - Don't return to the previous XY position after change.
 *   INDX still exits dock/cleaner service space through its safe parking route.
 * - `L` - Z Lift settings
 *   - `0` - no lift
 *   - `1` - lift by max MBL diff
 *   - `2` - full lift(default)
 * - `D` - Z lift return settings
 *   - `0` - Do not return in Z after lift
 *   - `1` - Normal return
 */
void PrusaGcodeSuite::P0() {

    static_assert(EXTRUDERS > 1);

    GcodeSuite::get_destination_from_command(); // sets destination = current position or user request

    // by default, Tx goes to specified destination or current position, unless following:
    tool_return_t return_type = tool_return_t::no_return;

    auto z_lift = static_cast<tool_change_lift_t>(parser.byteval('L', static_cast<uint8_t>(tool_change_lift_t::full_lift)));
    if (z_lift > tool_change_lift_t::_last_item) {
        z_lift = tool_change_lift_t::full_lift; // invalid input, use full_lift
    }
    bool z_down = parser.byteval('D', 1);
    if (!tool_change(NoTool {}, return_type, z_lift, z_down)) {
        return;
    }
#if HAS_INDX()
    // The last dock is beyond the printable X boundary (CORE One: X259).
    // Leave service space via the native dock/cleaner-aware route so the next
    // serial move need not bypass keep-out protection. Z remains unchanged.
    if (current_position.x > X_WASTEBIN_SAFE_POINT || current_position.y < Y_DOCK_PARKING_MIN_SAFE_POS) {
        mapi::park({
            .x = std::min(current_position.x, float(X_WASTEBIN_SAFE_POINT)),
            .y = std::max(current_position.y, float(Y_DOCK_PARKING_MIN_SAFE_POS)),
        });
    }
#endif
}
/** @}*/

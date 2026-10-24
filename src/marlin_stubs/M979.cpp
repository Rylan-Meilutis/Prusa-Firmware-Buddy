#include "PrusaGcodeSuite.hpp"
#include <Marlin/src/gcode/gcode.h>
#include <feature/dock_cooldown/controller.hpp>

void PrusaGcodeSuite::M979() {
    // Bare M979 is a passive status query. Validate the whole request before
    // changing mode; a malformed disable must never bypass a cooling hold.
    if (!parser.seenval('S')) {
        if (parser.seen('S') || parser.seen('C')) {
            SERIAL_ECHOLN("Error:M979 requires S0 or S1; C requires S1");
            return;
        }
        buddy::dock_cooldown::report();
        return;
    }
    const float mode = parser.floatval('S');
    const bool has_duration = parser.seen('C');
    const float seconds = parser.floatval('C', 180);
    if ((mode != 0 && mode != 1) || (has_duration && (!parser.seenval('C') || mode != 1))
        || !(seconds >= 1 && seconds <= 86400) || seconds != uint32_t(seconds)) {
        SERIAL_ECHOLN("Error:M979 expects S1 C1..86400 (whole seconds), or S0");
        return;
    }
    buddy::dock_cooldown::configure(mode == 1, uint32_t(seconds));
    buddy::dock_cooldown::report();
}

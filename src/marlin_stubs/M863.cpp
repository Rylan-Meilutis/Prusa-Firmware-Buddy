
#include "core/serial.h"
#include "gcode/parser.h"
#include "inc/MarlinConfig.h"
#include "PrusaGcodeSuite.hpp"
#include "module/prusa/tool_mapper.hpp"

#if ENABLED(PRUSA_TOOL_MAPPING)

void rme_report_tool_mapping() {
    SERIAL_ECHOPGM("RME_TOOLMAP "); SERIAL_ECHO(tool_mapper.is_enabled() ? 1 : 0);
    for (uint8_t logical = 0; logical < EXTRUDERS; ++logical) {
        SERIAL_ECHOPGM(" L"); SERIAL_ECHO(logical); SERIAL_CHAR('=');
        const uint8_t physical = tool_mapper.to_physical(logical, true);
        if (physical == ToolMapper::NO_TOOL_MAPPED) SERIAL_ECHO(-1); else SERIAL_ECHO(physical);
    }
    SERIAL_EOL();
}

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M863: Tool remapping <a href="https://reprap.org/wiki/G-code#M863_Tool_remapping">M863 Tool remapping</a>
 *
 * Only MK3.5/S, MK3.9/S, MK4/S with MMU and XL
 *
 *#### Usage
 *
 *    M863 [ M | L | P | E | R ]
 *
 *#### Parameters
 *
 * - `M` - Map needs `P' and `L`
 *   - `P` - Physical tool
 *   - `L` - Logical tool
 * - `E` - Set
 *    - `1` - enable
 *    - `0` - disable
 * - `R` - Reset to default
 *
 *#### Examples
 *
 *    M863 M P0 L1 ; Use tool 0 while in gcode tool 1 is selected
 *    M863 E1      ; Enable tool remapping
 *    M863 R       ; Reset tool remapping
 *    M863         ; Print current tool mapping
 *
 * Without parameters prints the current Tool mapping
 */
void PrusaGcodeSuite::M863() {
    if (parser.seen('M') && parser.seen("P") && parser.seen("L")) {
        // map logical tool to physical
        const uint8_t logical = parser.byteval('L');
        const uint8_t physical = parser.byteval('P');

        tool_mapper.set_mapping(logical, physical);
    } else if (parser.seen('E')) {
        // enable/disable tool mapping
        tool_mapper.set_enable(parser.boolval('E'));
    } else if (parser.seen('R')) {
        // reset tool mapping to default
        tool_mapper.reset();
    } else {
        rme_report_tool_mapping();
    }
}

/** @}*/

#endif

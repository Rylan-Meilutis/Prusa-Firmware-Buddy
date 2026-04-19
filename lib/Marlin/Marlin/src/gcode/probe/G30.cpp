/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfig.h"
#include "module/motion.h"

#if HAS_BED_PROBE

#include "../gcode.h"
#include "../../module/motion.h"
#include "../../module/probe.h"
#include "../../feature/bedlevel/bedlevel.h"
#include <tool_index.hpp>
#include <option/has_indx.h>
#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
  #include <module/prusa/tool_offset.hpp>
  #include <config_store/store_instance.hpp>
#endif

/** \addtogroup G-Codes
 * @{
 */

/**
 *### G30: Do a single Z probe at the current XY <a href="https://reprap.org/wiki/G-code#G30:_Single_Z-Probe">G30: Single Z-Probe</a>
 *
 *#### Usage
 *
 *    G30 [ X | Y | E | O | S ]
 *
 *#### Parameters
 *
 *  - `X` - Probe X position (default current X)
 *  - `Y` - Probe Y position (default current Y)
 *  - `E` - Engage the probe for each probe (default 1)
 *  - `O` - Safe Z tool offset of current tool (relative to tool 0)
 *  - `S` - Required number of successful probes to average (default 1)
 */
void GcodeSuite::G30() {
  const xy_pos_t pos = { parser.linearval('X', current_position.x + probe_offset.x + TERN0(HAS_HOTEND_OFFSET, hotend_currently_applied_offset.x)),
                         parser.linearval('Y', current_position.y + probe_offset.y + TERN0(HAS_HOTEND_OFFSET, hotend_currently_applied_offset.y)) };

  if (!position_is_reachable_by_probe(pos)) return;

  // Disable leveling so the planner won't mess with us
  #if HAS_LEVELING
    set_bed_leveling_enabled(false);
  #endif

  remember_feedrate_scaling_off();

  const ProbePtRaise raise_after = parser.boolval('E', true) ? PROBE_PT_STOW : PROBE_PT_NONE;
  const uint8_t required_successes = parser.byteval('S', 1);
  const float measured_z = probe_at_point(pos, raise_after, 1, true, required_successes);
  if (!isnan(measured_z))
    SERIAL_ECHOLNPAIR("Bed X: ", FIXFLOAT(pos.x), " Y: ", FIXFLOAT(pos.y), " Z: ", FIXFLOAT(measured_z), " (avg of ", required_successes, ")");

  restore_feedrate_and_scaling();

  #ifdef Z_AFTER_PROBING
    if (raise_after == PROBE_PT_STOW) move_z_after_probing();
  #endif

  #if HAS_INDX()
  if (!isnan(measured_z) && parser.boolval('O', false)) {
    static float master_tool_z = 0.0f;
    // Strip the currently applied tool offset from the measurement so that
    // recalibration doesn't accumulate the previous offset on top of itself.
    const float raw_z = measured_z - TERN0(HAS_HOTEND_OFFSET, hotend_currently_applied_offset.z);
    match(
      PhysicalToolIndex::currently_selected(),
      [&](PhysicalToolIndex i) {
        if (i == PhysicalToolIndex::from_raw(0)) {
          master_tool_z = raw_z;
        } else {
          ToolOffset of = config_store().get_tool_offset(i);
          of.z = master_tool_z - raw_z;
          config_store().set_tool_offset(i, of);
          hotend_offset[i].z = of.z;
          SERIAL_ECHOLNPAIR("Tool ", i.to_raw(), " Z offset: ", of.z);
        }
        SERIAL_ECHOLNPAIR("Tool ", i.to_raw(), " Z measured: ", measured_z);
      },
      [](NoTool) { return; }
    );
  }
  #endif
  report_current_position();
}

/** @}*/

#endif // HAS_BED_PROBE

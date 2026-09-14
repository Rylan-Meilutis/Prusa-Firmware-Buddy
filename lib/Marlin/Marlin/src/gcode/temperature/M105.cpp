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

#include "../gcode.h"
#include "../../module/motion.h"
#include "../../module/temperature.h"
#include <utils/variant_utils.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M105: Read hot end and bed temperature <a href="https://reprap.org/wiki/G-code#M105:_Get_Extruder_Temperature">M105: Get Extruder Temperature</a>
 *
 * Request a temperature report to be sent to the host as soon as possible.
 *
 *#### Usage
 *
 *    M105 [ T ]
 *
 *#### Parameters
 *
 * - `T` - Tool
 */
void GcodeSuite::M105() {

  // A toolchanger can legitimately have every tool parked.  M105 without an
  // explicit T parameter is still a status query in that state; do not route
  // it through get_target_physical_from_command(), which diagnoses NoTool as
  // "Invalid extruder -1" and makes hosts such as OctoPrint permanently mark
  // T0 invalid.  The temperature reporter already understands Marlin's
  // no-tool sentinel and reports the shared nozzle plus all physical channels
  // as unavailable while retaining bed/chamber temperatures.
  if (!parser.seenval('T') && std::holds_alternative<NoTool>(PhysicalToolIndex::currently_selected())) {
    SERIAL_ECHOPGM(MSG_OK);

    #if HAS_TEMP_SENSOR
      thermalManager.print_heater_states(active_extruder);
      SERIAL_EOL();
    #else
      SERIAL_ECHOLNPGM(" T:0");
    #endif
    return;
  }

  const std::optional<PhysicalToolIndex> tool = stdext::get_optional<PhysicalToolIndex>(get_target_physical_from_command());
  if (!tool.has_value()) return;

  SERIAL_ECHOPGM(MSG_OK);

  #if HAS_TEMP_SENSOR

    thermalManager.print_heater_states(*tool);

    SERIAL_EOL();

  #else
    // #error dead code found by automatic analyses (see BFW-5461)

    SERIAL_ECHOLNPGM(" T:0"); // Some hosts send M105 to test the serial connection

  #endif
}

/** @}*/

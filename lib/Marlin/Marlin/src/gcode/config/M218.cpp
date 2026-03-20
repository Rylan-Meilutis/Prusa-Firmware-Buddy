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

#if HAS_HOTEND_OFFSET

#include "../gcode.h"
#include "../../module/motion.h"
#include <utils/variant_utils.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M218: Get/Set hotend offset <a href="https://reprap.org/wiki/G-code#M218:_Set_Hotend_Offset">M218: Set Hotend Offset</a>
 *
 *#### Usage
 *
 *    M218 [ X | Y | Z ]
 *
 *#### Parameters
 *
 *
 * - `T` - Tool
 * - `X` - X hotend offset
 * - `Y` - Y hotend offset
 * - `Z` - Z hotend offset
 *
 * Without parameters prints the current hotend offset(s)
 */
void GcodeSuite::M218() {

  const std::optional<VirtualToolIndex> virtual_tool = stdext::get_optional<VirtualToolIndex>(get_target_virtual_from_command());
  // FIXME: The virtual_tool->to_raw() >= PhysicalToolIndex::count is clearly wrong.
  // Will be fixed in some follow-up commit.
  if (!virtual_tool.has_value() || virtual_tool->to_raw() >= PhysicalToolIndex::count) {
    SERIAL_ECHOLNPAIR("M218 wrong tool: ", virtual_tool.has_value() ? (int)virtual_tool->to_raw() : -1);
    return;
  }
  auto tool = PhysicalToolIndex::from_raw(virtual_tool->to_raw());

  if (parser.seenval('X')) hotend_offset[tool].x = parser.value_linear_units();
  if (parser.seenval('Y')) hotend_offset[tool].y = parser.value_linear_units();
  if (parser.seenval('Z')) hotend_offset[tool].z = parser.value_linear_units();

  if (!parser.seen("XYZ")) {
    SERIAL_ECHO_START();
    SERIAL_ECHOPGM(MSG_HOTEND_OFFSET);
    for (auto tool : PhysicalToolIndex::all()) {
      SERIAL_CHAR(' ');
      SERIAL_ECHO(hotend_offset[tool].x);
      SERIAL_CHAR(',');
      SERIAL_ECHO(hotend_offset[tool].y);
      SERIAL_CHAR(',');
      SERIAL_ECHO_F(hotend_offset[tool].z, 3);
    }
    SERIAL_EOL();
  }
}

/** @}*/

#endif // HAS_HOTEND_OFFSET

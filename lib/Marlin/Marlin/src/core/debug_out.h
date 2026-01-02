/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

//
// Serial aliases for debugging.
// Include this header (or not) in a given .cpp file
//

#undef DEBUG_SECTION
#undef DEBUG_ECHO_START
#undef DEBUG_ERROR_START
#undef DEBUG_CHAR
#undef DEBUG_ECHO
#undef DEBUG_DECIMAL
#undef DEBUG_ECHO_F
#undef DEBUG_ECHOLN
#undef DEBUG_ECHOPGM
#undef DEBUG_ECHOLNPGM
#undef DEBUG_ECHOF
#undef DEBUG_ECHOLNF
#undef DEBUG_ECHOPGM_P
#undef DEBUG_ECHOLNPGM_P
#undef DEBUG_ECHOPAIR
#undef DEBUG_ECHOPAIR_F
#undef DEBUG_ECHOPAIR_F_P
#undef DEBUG_ECHOLNPAIR
#undef DEBUG_ECHOLNPAIR_F
#undef DEBUG_ECHOLNPAIR_F_P
#undef DEBUG_ECHO_MSG
#undef DEBUG_ERROR_MSG
#undef DEBUG_EOL
#undef DEBUG_FLUSH
#undef DEBUG_POS
#undef DEBUG_XYZ
#undef DEBUG_DELAY
#undef DEBUG_SYNCHRONIZE

#define DEBUG_SECTION(...)        NOOP
#define DEBUG_ECHO_START()        NOOP
#define DEBUG_ERROR_START()       NOOP
#define DEBUG_CHAR(...)           NOOP
#define DEBUG_ECHO(...)           NOOP
#define DEBUG_DECIMAL(...)        NOOP
#define DEBUG_ECHO_F(...)         NOOP
#define DEBUG_ECHOLN(...)         NOOP
#define DEBUG_ECHOPGM(...)        NOOP
#define DEBUG_ECHOLNPGM(...)      NOOP
#define DEBUG_ECHOF(...)          NOOP
#define DEBUG_ECHOLNF(...)        NOOP
#define DEBUG_ECHOPGM_P(...)      NOOP
#define DEBUG_ECHOLNPGM_P(...)    NOOP
#define DEBUG_ECHOPAIR(...)       NOOP
#define DEBUG_ECHOPAIR_F(...)     NOOP
#define DEBUG_ECHOPAIR_F_P(...)   NOOP
#define DEBUG_ECHOLNPAIR(...)     NOOP
#define DEBUG_ECHOLNPAIR_F(...)   NOOP
#define DEBUG_ECHOLNPAIR_F_P(...) NOOP
#define DEBUG_ECHO_MSG(...)       NOOP
#define DEBUG_ERROR_MSG(...)      NOOP
#define DEBUG_EOL()               NOOP
#define DEBUG_FLUSH()             NOOP
#define DEBUG_POS(...)            NOOP
#define DEBUG_XYZ(...)            NOOP
#define DEBUG_DELAY(...)          NOOP
#define DEBUG_SYNCHRONIZE()       NOOP

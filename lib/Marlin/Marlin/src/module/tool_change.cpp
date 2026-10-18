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

#include "../inc/MarlinConfigPre.h"

#include "tool_change.h"
#include <serial_printing.hpp>
#include <option/has_indx.h>
#include <option/has_loadcell.h>
#if ENABLED(PRUSA_MMU2)
#include <feature/prusa/MMU2/mmu2_mk4.h>
#endif
#if HAS_LOADCELL()
  #include <common/feature/extrusion_calibration.hpp>
  #include <feature/pressure_advance/pressure_advance_config.hpp>

static uint8_t pa_loaded_slot() {
#if ENABLED(PRUSA_MMU2)
    if (MMU2::mmu2.Enabled()) return MMU2::mmu2.get_current_tool();
#endif
    return active_extruder;
}

void buddy::extrusion_calibration::apply_selected_indx_result() {
    const auto slot = pa_loaded_slot();
    planner.synchronize();
    if (planner.draining()) return;
    select_job_result(slot);
    if (const auto *result = job_result(slot)) {
        pressure_advance::set_axis_e_config({ result->pressure_advance, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(slot, result->max_flow_mm3_s);
    }
}

void buddy::extrusion_calibration::apply_print_temperature(uint8_t physical_tool, int16_t temperature) {
    if (calibration_command_active()) return;
    const auto slot = pa_loaded_slot();
    const auto selected_physical = ENABLED(PRUSA_MMU2) ? 0 : active_extruder;
    if (physical_tool != selected_physical || slot >= max_logical_filaments) return;
    if (const auto *current = job_result(slot); current && current->calibration_temperature == temperature) return;
    if (!request_job_temperature(slot, temperature)) return;
    apply_selected_indx_result();
    const auto *result = job_result(slot);
    SERIAL_ECHOLNPAIR("PA_TEMPERATURE slot=", slot, " temperature=", temperature, " result_pa=", result->pressure_advance);
}
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
  #include <module/prusa/toolchanger.h>
#endif

#include <option/has_mmu2.h>
#if HAS_MMU2()
  #include <feature/prusa/MMU2/mmu2_mk4.h>
#endif

#if EXTRUDERS > 1
  toolchange_settings_t toolchange_settings;  // Initialized by settings.load()
#endif

#if ENABLED(SINGLENOZZLE)
  uint16_t singlenozzle_temp[EXTRUDERS];
  #if FAN_COUNT > 0
    uint8_t singlenozzle_fan_speed[EXTRUDERS];
  #endif
#endif

#if HAS_LEVELING
  #include "../feature/bedlevel/bedlevel.h"
#endif

/**
 * Perform a tool-change, which may result in moving the
 * previous tool out of the way and the new tool into place.
 */
void tool_change(const uint8_t new_tool,
                 tool_return_t return_type /*= tool_return_t::to_current*/,
                 tool_change_lift_t z_lift /*= tool_change_lift_t::full_lift*/,
                 bool z_return /*= true*/){

  #if ENABLED(PRUSA_MMU2)
    (void) return_type;
    (void) z_lift;
    (void) z_return;
    MMU2::mmu2.tool_change(new_tool);
    #if HAS_LOADCELL()
      buddy::extrusion_calibration::apply_selected_indx_result();
    #endif

  #elif HAS_TOOLCHANGER()
    SerialPrinting::notify_workflow("tool_change", "open", "Tool change in progress");
    SerialPrinting::notify_status("Tool change in progress", -1, true);
    const bool success = prusa_toolchanger.tool_change(new_tool, return_type, current_position, z_lift, z_return);
    if (!success) {
      SerialPrinting::notify_error("tool_change", "tool_change_failed", "Tool change failed");
    } else {
      SerialPrinting::notify_workflow("tool_change", "closed", "Tool change complete", 100);
    }

  #else
    #error Not implemented
  #endif
}

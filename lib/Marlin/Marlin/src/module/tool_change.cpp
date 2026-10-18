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
#include <utils/overloaded_visitor.hpp>
#include <mapi/motion.hpp>
#include <serial_printing.hpp>
#include <option/has_indx.h>
#include <option/has_loadcell.h>
#include <option/has_mmu2.h>
#if HAS_LOADCELL()
  #include <feature/extrusion_calibration.hpp>
  #include <feature/pressure_advance/pressure_advance_config.hpp>

void buddy::extrusion_calibration::apply_selected_indx_result() {
    const auto logical = VirtualToolIndex::currently_selected_opt();
    const size_t slot = logical ? logical->to_raw() : max_logical_filaments;
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
    for (const auto logical : VirtualToolIndex::all().skip_all_disabled()) {
        if (logical.to_physical().to_raw() != physical_tool) continue;
        #if HAS_MMU2()
          // All MMU slots share hotend 0. Its target belongs only to the
          // loaded filament, not every calibrated but currently parked slot.
          if (VirtualToolIndex::currently_selected_opt() != logical) continue;
        #endif
        if (const auto *current = job_result(logical.to_raw()); current && current->calibration_temperature == temperature) continue;
        if (!request_job_temperature(logical.to_raw(), temperature)) continue;
        if (VirtualToolIndex::currently_selected_opt() == logical) {
            apply_selected_indx_result();
            const auto *result = job_result(logical.to_raw());
            SERIAL_ECHOLNPAIR("PA_TEMPERATURE slot=", logical.to_raw(), " temperature=", temperature, " result_pa=", result->pressure_advance);
        }
    }
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
  uint8_t singlenozzle_fan_speed[EXTRUDERS];
#endif

#if HAS_LEVELING
  #include "../feature/bedlevel/bedlevel.h"
#endif

/**
 * Perform a tool-change, which may result in moving the
 * previous tool out of the way and the new tool into place.
 * Returns true on success, false if the toolchanger reported a failure.
 */
bool tool_change(const std::variant<VirtualToolIndex, PhysicalToolIndex, NoTool> new_tool,
                 [[maybe_unused]] tool_return_t return_type /*= tool_return_t::to_current*/,
                 [[maybe_unused]] tool_change_lift_t z_lift /*= tool_change_lift_t::full_lift*/,
                 [[maybe_unused]] bool z_return /*= true*/){

  #if HAS_MMU2()
    match(new_tool,
      [](VirtualToolIndex virtual_tool){ MMU2::mmu2.tool_change(virtual_tool.to_raw()); },
      [](PhysicalToolIndex physical_tool){ /* do nothing */ static_assert(PhysicalToolIndex::count == 1); },
      [](NoTool){ MMU2::mmu2.unload(); }
    );
    #if HAS_LOADCELL()
      buddy::extrusion_calibration::apply_selected_indx_result();
    #endif
    return true;

  #elif HAS_TOOLCHANGER()
    SerialPrinting::notify_workflow("tool_change", "open", "Tool change in progress");
    SerialPrinting::notify_status("Tool change in progress", -1, true);
    using MaybePhysical = std::variant<PhysicalToolIndex, NoTool>;
    auto maybe_physical = match(new_tool,
      [](VirtualToolIndex virtual_tool) -> MaybePhysical { return virtual_tool.to_physical(); },
      [](PhysicalToolIndex physical_tool) -> MaybePhysical { return physical_tool; },
      [](NoTool) -> MaybePhysical { return NoTool{}; }
    );
    const bool success = prusa_toolchanger.tool_change(maybe_physical, return_type, current_position.xyz(), z_lift, z_return);
    #if HAS_LOADCELL()
      if (success) buddy::extrusion_calibration::apply_selected_indx_result();
    #endif
    if (!success) SerialPrinting::notify_error("tool_change", "tool_change_failed", "Tool change failed");
    else SerialPrinting::notify_workflow("tool_change", "closed", "Tool change complete", 100);
    return success;

  #else
    #error Not implemented
  #endif
}

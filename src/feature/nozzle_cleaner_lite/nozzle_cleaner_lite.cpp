#include "include/nozzle_cleaner_lite.hpp"
#include <printers.h>
#include "Marlin/src/gcode/gcode.h"
#include <Marlin/src/Marlin.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/probe.h>
#include <Marlin/src/module/temperature.h>
#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <Marlin/src/module/tool_change.h>
    #include <module/prusa/toolchanger.h>
    #include <gcode/gcode_info.hpp>
#endif
#include <option/has_spool_join.h>
#if HAS_SPOOL_JOIN()
    #include <module/prusa/spool_join.hpp>
#endif
#include <tool/hotend/hotend.hpp>
#if 0
    // Commented out (not deleted): tool_offset::wait_for_loadcell_alive() no
    // longer exists upstream - HAS_TOOL_OFFSET_SENSOR dropped plain XL, keeping
    // it for COREONE_INDX/COREONEL_INDX only, so this header isn't even linked
    // for XL anymore. run_z_probe() (probe.cpp, BFW-8854) now waits for fresh
    // loadcell samples before every probe on every printer, making this XL-only
    // guard redundant. Reverse only if XL regains its own tool-offset sensor.
    #include <Marlin/src/feature/contactless_offset/contactless_offset.hpp>
#endif
#include <Marlin/src/feature/pressure_advance/pressure_advance_config.hpp>
#include "loadcell.hpp"
#include <feature/print_status_message/print_status_message_guard.hpp>
#include <logging/log.hpp>
#include <raii/scope_guard.hpp>
#include <config_store/store_definition.hpp>
#include <bsod/bsod.h>

#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>

LOG_COMPONENT_DEF(NozzleCleanerLite, logging::Severity::info);

namespace nozzle_cleaner_lite {

namespace {
    enum class CleanerAxis : uint8_t {
        x,
        y,
    };

#if PRINTER_IS_PRUSA_XL()
    // Touchpoint: fixed reference point next to the cleaner, used to home Z
    // locally instead of relying on G28's (possibly distant) safe-homing XY.
    constexpr xy_pos_t touchpoint_xy = { { { -6.45f, 70.0f } } };

    constexpr CleanerAxis cleaner_axis = CleanerAxis::y;
    constexpr float cleaner_distance = 10.0f;
    constexpr float cleaner_length = 30.0f;
#elif PRINTER_IS_PRUSA_COREONE()
    constexpr xy_pos_t touchpoint_xy = { { { 206.5f, -15.0f } } };

    constexpr CleanerAxis cleaner_axis = CleanerAxis::x;
    constexpr float cleaner_distance = -10.0f;
    constexpr float cleaner_length = -30.0f;
#elif PRINTER_IS_PRUSA_COREONEL()
    constexpr xy_pos_t touchpoint_xy = { { { 299.5f, -7.5f } } };

    constexpr CleanerAxis cleaner_axis = CleanerAxis::x;
    constexpr float cleaner_distance = -10.0f;
    constexpr float cleaner_length = -30.0f;
#else
    #error "nozzle_cleaner_lite sequence not defined for this printer variant"
#endif

    constexpr float cleaner_x_near = touchpoint_xy.x + (cleaner_axis == CleanerAxis::x ? cleaner_distance : 0.0f);
    constexpr float cleaner_y_near = touchpoint_xy.y + (cleaner_axis == CleanerAxis::y ? cleaner_distance : 0.0f);
    constexpr float cleaner_x_far = cleaner_x_near + (cleaner_axis == CleanerAxis::x ? cleaner_length : 0.0f);
    constexpr float cleaner_y_far = cleaner_y_near + (cleaner_axis == CleanerAxis::y ? cleaner_length : 0.0f);

    // Z targets expressed relative to the freshly probed touchpoint surface, so
    // they stay correct even if the Z home offset or touchpoint height drifts.
    constexpr float safe_above_surface_mm = 1.0f;
    // Rough expected touchpoint surface height in machine Z; the surface sits
    // slightly below the homed bed level. Only needs to be near-right:
    // run_z_probe approaches from above and searches down to
    // expected + Z_PROBE_LOW_POINT, so the true surface is found each run.
    constexpr float expected_touchpoint_surface_z = 0.0f;
    constexpr float dive_below_surface_mm = -0.7f;
    constexpr float travel_clearance_mm = 10.0f;

    constexpr feedRate_t approach_feedrate = MMM_TO_MMS(1200);
    constexpr feedRate_t dive_feedrate = MMM_TO_MMS(6000);
    constexpr feedRate_t rub_feedrate_fast = MMM_TO_MMS(8000);
    constexpr feedRate_t rub_feedrate_slow = MMM_TO_MMS(1500);

    constexpr uint8_t rub_cycles_fast = 5;
    constexpr uint8_t rub_cycles_slow = 2;

    float probe_touchpoint_z() {
        // XL-only staleness guard removed; see the commented-out
        // contactless_offset.hpp include above for why and when to reverse.
#if 0
        if (!tool_offset::wait_for_loadcell_alive()) {
            log_error(NozzleCleanerLite, "Loadcell did not produce fresh samples before touchpoint probe");
            return std::numeric_limits<float>::quiet_NaN();
        }
#endif

        pressure_advance::PressureAdvanceDisabler pa_disabler;
        Loadcell::HighPrecisionEnabler loadcell_high_precision_enabler(loadcell);
        return probe_here(expected_touchpoint_surface_z);
    }

    // Move in raw machine coordinates (line_to_machine_pos), bypassing MBL.
    void move_to_machine_pos_xy(float x, float y, feedRate_t fr_mm_s) {
        auto target = current_machine_position();
        target.x = x;
        target.y = y;
        line_to_machine_pos(target, fr_mm_s);
        planner.synchronize();
    }

    void move_to_machine_pos_z(float z, feedRate_t fr_mm_s) {
        auto target = current_machine_position();
        target.z = z;
        line_to_machine_pos(target, fr_mm_s);
        planner.synchronize();
    }

} // namespace

bool is_available() {
    return config_store().nozzle_cleaner_lite_installed.get();
}

void clean() {
    release_assert(is_available());

    PrintStatusMessageGuard status_message;
    status_message.update<PrintStatusMessage::nozzle_cleaner_lite>({});

    const std::optional<PhysicalToolIndex> tool = PhysicalToolIndex::currently_selected_opt();
    if (!tool) {
        log_error(NozzleCleanerLite, "no tool selected");
        return;
    }

    const FilamentType filament = FilamentType::for_tool_heuristic(tool->currently_selected_virtual_tool());
    const int16_t cleaning_temperature = filament ? filament.parameters().nozzle_temperature - cleaning_temp_diff : fallback_cleaning_temperature;

    // Start heating used tool and restore target temp on exit
    const int16_t saved_nozzle_target = Hotend::for_tool(*tool).nozzle_target_temp();
    Hotend::for_tool(*tool).set_nozzle_target_temp(cleaning_temperature);
    ScopeGuard restore_nozzle_target([&] {
        Hotend::for_tool(*tool).set_nozzle_target_temp(saved_nozzle_target);
    });

    // Home XY (and Z) only if needed, with clearance so we don't drag over the bed.
    if (!GcodeSuite::G28_no_parser(true, true, true, G28Flags { .only_if_needed = true, .z_raise = travel_clearance_mm })) {
        log_error(NozzleCleanerLite, "homing failed");
        return;
    }

    move_to_machine_pos_xy(touchpoint_xy.x, touchpoint_xy.y, dive_feedrate);

    // No blind descent here: the touchpoint surface height is not known yet, so let
    // run_z_probe approach from above and find it (it stops on contact even
    // during the fast approach).
    const float probed_z = probe_touchpoint_z();
    if (std::isnan(probed_z)) {
        log_error(NozzleCleanerLite, "Touchpoint probe failed");
        return;
    }
    log_info(NozzleCleanerLite, "Touchpoint surface at Z=%.3f", static_cast<double>(probed_z));

    move_to_machine_pos_z(probed_z + safe_above_surface_mm, approach_feedrate);

    if (!thermalManager.wait_for_hotend(*tool, { .no_wait_for_cooling = true })) {
        log_error(NozzleCleanerLite, "heating failed");
        return;
    }

    // Safely move from the touchpoint to the cleaner
    move_to_machine_pos_xy(cleaner_x_near, cleaner_y_near, approach_feedrate);
    move_to_machine_pos_z(probed_z + dive_below_surface_mm, dive_feedrate);

    // Rub: a few fast cycles, then a couple of slower ones to finish cleanly
    for (uint8_t i = 0; i < rub_cycles_fast; ++i) {
        move_to_machine_pos_xy(cleaner_x_far, cleaner_y_far, rub_feedrate_fast);
        move_to_machine_pos_xy(cleaner_x_near, cleaner_y_near, rub_feedrate_fast);
    }
    for (uint8_t i = 0; i < rub_cycles_slow; ++i) {
        move_to_machine_pos_xy(cleaner_x_far, cleaner_y_far, rub_feedrate_slow);
        move_to_machine_pos_xy(cleaner_x_near, cleaner_y_near, rub_feedrate_slow);
    }

    // Retreat back over the touchpoint
    move_to_machine_pos_z(probed_z + travel_clearance_mm, rub_feedrate_fast);
    move_to_machine_pos_xy(touchpoint_xy.x, touchpoint_xy.y, rub_feedrate_fast);
}

#if HAS_TOOLCHANGER()
static void clean_before_probing_toolchanger() {
    std::bitset<PhysicalToolIndex::count> used_physical_tools;

    // Walk each gcode tool that is used in the print
    auto &gcode_info = GCodeInfo::getInstance();
    for (auto gcode_tool : GcodeToolIndex::all()) {
        if (!gcode_info.get_extruder_info(gcode_tool).used()) {
            continue;
        }

        // Map gcode → virtual (respects tool mapper)
        auto virtual_tool = stdext::get_optional<VirtualToolIndex>(gcode_tool.to_virtual());
        while (virtual_tool) {
            used_physical_tools.set(virtual_tool->to_physical().to_raw());

    #if HAS_SPOOL_JOIN()
            virtual_tool = spool_join.get_spool_2(*virtual_tool);
    #else
            virtual_tool = std::nullopt;
    #endif
        }
    }

    // The start gcode deliberately selects the tool that homes and probes MBL
    // before the G29 that triggers cleaning; put it back once the cleaning
    // tool changes are done so probing runs with the tool the gcode chose.
    const auto original_tool = PhysicalToolIndex::currently_selected();
    ScopeGuard restore_original_tool([&] {
        if (PhysicalToolIndex::currently_selected() != original_tool
            && !tool_change(stdext::to_variant(original_tool), tool_return_t::no_return)) {
            log_error(NozzleCleanerLite, "Failed to restore originally selected tool");
        }
    });

    // Start heating all used tools to the cleaning temperature in parallel to
    // save time; restore the targets once all of them are cleaned
    std::array<int16_t, PhysicalToolIndex::count> saved_nozzle_targets {};
    for (auto tool : PhysicalToolIndex::all().skip_all_disabled()) {
        if (!used_physical_tools.test(tool.to_raw())) {
            continue;
        }
        const FilamentType filament = FilamentType::for_tool_heuristic(tool.currently_selected_virtual_tool());
        const int16_t cleaning_temperature = filament ? filament.parameters().nozzle_temperature - cleaning_temp_diff : fallback_cleaning_temperature;

        saved_nozzle_targets[tool.to_raw()] = Hotend::for_tool(tool).nozzle_target_temp();
        Hotend::for_tool(tool).set_nozzle_target_temp(cleaning_temperature);
    }
    ScopeGuard restore_nozzle_targets([&] {
        for (auto tool : PhysicalToolIndex::all().skip_all_disabled()) {
            if (used_physical_tools.test(tool.to_raw())) {
                Hotend::for_tool(tool).set_nozzle_target_temp(saved_nozzle_targets[tool.to_raw()]);
            }
        }
    });

    for (auto tool : PhysicalToolIndex::all().skip_all_disabled()) {
        if (!used_physical_tools.test(tool.to_raw())) {
            continue;
        }

        if (!tool_change(stdext::to_variant(tool), tool_return_t::no_return)) {
            log_error(NozzleCleanerLite, "Tool change to tool %u failed", tool.to_raw());
            break;
        }
        clean();
    }
}
#endif

void clean_before_probing(Badge<unified_bed_leveling>) {
    release_assert(is_available());

#if !HAS_TOOLCHANGER()
    clean();
#else
    clean_before_probing_toolchanger();
#endif

    // MBL probing follows right after this returns, with no M109 in between
    // to re-stabilize the nozzle. Cleaning may have left it above its
    // restored target (e.g. ASA, cleaning temp > MBL temp) or below it (e.g.
    // PLA); wait for it to settle back so probing runs at the temperature
    // the start gcode set, not mid-cleaning.
    const auto active_tool = PhysicalToolIndex::currently_selected_opt();
    if (active_tool && Hotend::for_tool(*active_tool).nozzle_target_temp() > 0) {
        thermalManager.wait_for_hotend(*active_tool, { .no_wait_for_cooling = false });
    }
}

} // namespace nozzle_cleaner_lite

#include "PrusaGcodeSuite.hpp"

#include <Marlin/src/feature/pressure_advance/pressure_advance.hpp>
#include <Marlin/src/feature/pressure_advance/pressure_advance_config.hpp>
#include <Marlin/src/feature/prusa/e-stall_detector.h>
#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/probe.h>
#include <Marlin/src/module/temperature.h>
#include <common/feature/extrusion_calibration.hpp>
#include <common/mapi/motion.hpp>
#include <common/mapi/parking.hpp>
#include <config_store/store_instance.hpp>
#include <loadcell.hpp>
#include <option/has_wastebin.h>
#include <printers.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr float filament_area = 2.40528f;
constexpr float absolute_pa_max = 0.15f;
#if !HAS_WASTEBIN()
constexpr float mesh_y_spacing = float(MESH_MAX_Y - MESH_MIN_Y) / float(GRID_MAX_POINTS_Y - 1);
static_assert(MESH_MIN_Y + mesh_y_spacing * GRID_BORDER > 7.0f,
    "M976 anchor strip overlaps the first probed UBL row");
#endif

float material_fallback(const uint8_t logical_filament) {
    const auto &name = config_store().get_filament_type(logical_filament).parameters().name;
    if (!strncmp(name.data(), "FLEX", 4)) return 0.08f;
    if (!strncmp(name.data(), "PETG", 4)) return 0.045f;
    if (!strncmp(name.data(), "PA", 2)) return 0.05f;
    return 0.04f;
}

void extrude_flow(const float flow_mm3_s, const float seconds) {
    const float filament_speed = flow_mm3_s / filament_area;
    mapi::extruder_move(filament_speed * seconds, filament_speed, true);
}

buddy::extrusion_calibration::Score run_bursts(const float pa) {
    // Match PrusaPATuner's asymmetric 0.8/8.0 mm/s filament square wave.
    // Queue the complete burst before synchronizing: PA must see continuous
    // slow/fast transitions, not a stop between every leg.
    constexpr float slow_flow = 0.8f * filament_area;
    constexpr float fast_flow = 8.0f * filament_area;
    pressure_advance::set_axis_e_config({ pa, pressure_advance::get_axis_e_config().smooth_time });
    auto &capture = buddy::extrusion_calibration::capture();
    capture.start();
    pressure_advance::set_calibration_mode(true);
    for (uint8_t cycle = 0; cycle < 4 && !planner.draining(); ++cycle) {
        extrude_flow(slow_flow, 1.0f);
        extrude_flow(fast_flow, 0.25f);
    }
    planner.synchronize();
    pressure_advance::set_calibration_mode(false);
    capture.stop();
    return capture.score();
}

float material_flow_limit(const uint8_t logical_filament) {
    const auto &name = config_store().get_filament_type(logical_filament).parameters().name;
    if (!strncmp(name.data(), "FLEX", 4)) return 4.0f;
    if (!strncmp(name.data(), "PETG", 4)) return 10.0f;
    if (!strncmp(name.data(), "PA", 2) || !strncmp(name.data(), "PC", 2)) return 8.0f;
    return 12.0f;
}

float probe_anchor_slot(const uint8_t slot) {
#if HAS_WASTEBIN()
    (void)slot;
    return NAN;
#else
    const float x = 8.0f + slot * 11.0f;
    constexpr float y0 = 2.0f, y1 = 5.0f;
    const std::array<xy_pos_t, 4> points { xy_pos_t { x - 3, y0 }, { x + 3, y0 }, { x - 3, y1 }, { x + 3, y1 } };
    float z = 0;
    for (const auto &point : points) {
        const float measured = probe_at_point(point, PROBE_PT_STOW, 0);
        if (!std::isfinite(measured)) return NAN;
        z += measured;
    }
    return z / points.size();
#endif
}

void cleanup(const uint8_t slot, const float anchor_z) {
#if HAS_WASTEBIN()
    mapi::park(mapi::ZAction::move_to_at_least, mapi::park_positions[mapi::ParkPosition::purge]);
    mapi::extruder_move(-1.0f, 20.0f, true);
    planner.synchronize();
#else
    const float x = 8.0f + slot * 11.0f;
    do_blocking_move_to_z(std::max(current_position.z, 5.0f), 5.0f);
    do_blocking_move_to_xy(xy_pos_t { x - 3.0f, 3.5f }, 50.0f);
    do_blocking_move_to_z(anchor_z + 0.20f, 3.0f);
    auto pos = current_position;
    pos.x = x + 3.0f;
    pos.e += 1.2f;
    planner.buffer_line(pos, 8.0f, active_extruder);
    planner.synchronize();
    current_position = pos;
    mapi::extruder_move(-0.8f, 20.0f, true);
    planner.synchronize();
    do_blocking_move_to_z(anchor_z + 5.0f, 5.0f);
    buddy::extrusion_calibration::occupy_anchor(slot);
#endif
}

void park_for_free_air_calibration(const uint8_t slot) {
#if HAS_WASTEBIN()
    mapi::home_if_needed_and_park(mapi::ZAction::move_to_at_least, mapi::park_positions[mapi::ParkPosition::purge]);
#else
    // The front service travel is outside the printable Y range on MK4,
    // CORE One and XL. Keep X aligned with the later cleanup slot so hanging
    // strands from different logical filaments remain separated.
    const mapi::ParkingPosition position { 8.0f + slot * 11.0f, Y_MIN_POS + 1.0f, 10.0f };
    mapi::home_if_needed_and_park(mapi::ZAction::move_to_at_least, position);
#endif
}
} // namespace

void PrusaGcodeSuite::M976() {
#if !(PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX())
    SERIAL_ERROR_MSG("M976 unsupported printer");
    return;
#else
    const uint8_t tool = parser.byteval('T', active_extruder);
    const uint8_t slot = parser.byteval('L', tool);
    if (tool != active_extruder || slot >= buddy::extrusion_calibration::max_logical_filaments) {
        SERIAL_ERROR_MSG("M976 select requested tool/slot first");
        return;
    }
    if (parser.boolval('C', false)) {
        buddy::extrusion_calibration::clear_anchor(slot);
        SERIAL_ECHOLNPAIR("PA_CALIBRATION anchor slot cleared=", slot);
        return;
    }
    if (const auto *cached = buddy::extrusion_calibration::job_result(slot)) {
        pressure_advance::set_axis_e_config({ cached->pressure_advance, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(active_extruder, cached->max_flow_mm3_s);
        buddy::extrusion_calibration::configure_pressure_monitor(cached->pressure_reference, 0.8f, 8.0f);
        SERIAL_ECHOLNPAIR("PA_CALIBRATION cached result=", cached->pressure_advance, " max_flow=", cached->max_flow_mm3_s);
        return;
    }
    if (thermalManager.tooColdToExtrude(active_extruder)) {
        SERIAL_ERROR_MSG("M976 hotend too cold");
        return;
    }
#if !HAS_WASTEBIN()
    if (buddy::extrusion_calibration::occupied_anchor_mask() & (1u << slot)) {
        SERIAL_ERROR_MSG("M976 anchor slot occupied; remove debris and run M976 C L<slot>");
        return;
    }
#endif
    const float fallback = constrain(parser.floatval('B', buddy::extrusion_calibration::profile_pressure_advance_or(material_fallback(slot))), 0.0f, absolute_pa_max);
    if (!GcodeSuite::G28_no_parser(true, true, true)) {
        SERIAL_ERROR_MSG("M976 homing failed");
        return;
    }
    const float anchor_z = probe_anchor_slot(slot);
    if (!HAS_WASTEBIN() && !std::isfinite(anchor_z)) {
        SERIAL_ERROR_MSG("M976 anchor micro-mesh failed");
        return;
    }
    park_for_free_air_calibration(slot);
    BlockEStallDetection block_legacy_e_stall;
    buddy::extrusion_calibration::suspend_pressure_monitor(true);
    auto high_precision = Loadcell::HighPrecisionEnabler(loadcell, !loadcell.IsHighPrecisionEnabled());
    loadcell.WaitBarrier();
    loadcell.Tare();

    float best_pa = fallback, best_cost = std::numeric_limits<float>::infinity();
    buddy::extrusion_calibration::Score best_score;
    // Coarse search around the profile/material fallback, followed by the
    // PrusaPATuner 0.002-resolution refinement around the best response.
    for (int8_t offset = -3; offset <= 3; ++offset) {
        const float candidate = constrain(fallback + offset * 0.01f, 0.0f, absolute_pa_max);
        const auto score = run_bursts(candidate);
        if (score.valid && score.transient < best_cost) {
            best_cost = score.transient;
            best_pa = candidate;
            best_score = score;
        }
    }
    for (int8_t offset = -2; offset <= 2; ++offset) {
        const float candidate = constrain(best_pa + offset * 0.002f, 0.0f, absolute_pa_max);
        const auto score = run_bursts(candidate);
        if (score.valid && score.transient < best_cost) {
            best_cost = score.transient;
            best_pa = candidate;
            best_score = score;
        }
    }
    if (!std::isfinite(best_cost)) {
        pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(active_extruder, material_flow_limit(slot));
        cleanup(slot, anchor_z);
        buddy::extrusion_calibration::suspend_pressure_monitor(false);
        SERIAL_ERROR_MSG("M976 invalid loadcell signal; fallback applied");
        return;
    }

    const float max_flow = material_flow_limit(slot);
    const buddy::extrusion_calibration::Result result { best_pa, max_flow, 1.0f / (1.0f + best_cost), true, best_score };
    buddy::extrusion_calibration::set_job_result(slot, result);
    pressure_advance::set_axis_e_config({ best_pa, pressure_advance::get_axis_e_config().smooth_time });
    planner.set_max_volumetric_flow(active_extruder, max_flow);
    cleanup(slot, anchor_z);
    buddy::extrusion_calibration::configure_pressure_monitor(best_score, 0.8f, 8.0f);
    buddy::extrusion_calibration::suspend_pressure_monitor(false);
    SERIAL_ECHOLNPAIR("PA_CALIBRATION tool=", tool, " slot=", slot, " result=", best_pa, " max_flow=", max_flow, " confidence=", result.confidence);
#endif
}

#include "PrusaGcodeSuite.hpp"

#include <Marlin/src/feature/pressure_advance/pressure_advance.hpp>
#include <Marlin/src/feature/pressure_advance/pressure_advance_config.hpp>
#include <Marlin/src/feature/prusa/e-stall_detector.h>
#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/gcode/temperature/M104_M109.hpp>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/probe.h>
#include <Marlin/src/module/temperature.h>
#include <Marlin/src/module/tool_change.h>
#if ENABLED(PRUSA_MMU2)
#include <Marlin/src/feature/prusa/MMU2/mmu2_mk4.h>
#endif
#include <feature/extrusion_calibration.hpp>
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
#include <cstdio>
#include <cstring>

namespace {
constexpr float filament_area = 2.40528f;
constexpr float absolute_pa_max = 0.15f;
struct BatchEntry {
    uint8_t physical_tool;
    uint8_t logical_filament;
    int16_t temperature;
    std::array<char, 17> material {};
};

class HotendTargetRestorer {
public:
    HotendTargetRestorer() {
        for (uint8_t hotend = 0; hotend < HOTENDS; ++hotend) {
            targets_[hotend] = Temperature::degTargetHotend(PhysicalToolIndex::from_raw(hotend));
        }
    }
    ~HotendTargetRestorer() {
        for (uint8_t hotend = 0; hotend < HOTENDS; ++hotend) {
            Temperature::setTargetHotend(targets_[hotend], PhysicalToolIndex::from_raw(hotend));
        }
    }

private:
    std::array<int16_t, HOTENDS> targets_ {};
};

bool parse_uint(const char *&cursor, unsigned &value, const char terminator) {
    if (*cursor < '0' || *cursor > '9') return false;
    value = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + unsigned(*cursor++ - '0');
        if (value > 999) return false;
    }
    return *cursor++ == terminator;
}

size_t parse_batch_manifest(const char *command, std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> &entries) {
    const char *cursor = strstr(command, " A");
    if (!cursor) return 0;
    cursor += 2;
    while (*cursor == ' ') ++cursor;
    size_t count = 0;
    while (*cursor && count < entries.size()) {
        unsigned physical, logical, temperature;
        if (!parse_uint(cursor, physical, ':') || !parse_uint(cursor, logical, ':')) return 0;
        auto &entry = entries[count];
        size_t material_length = 0;
        while (*cursor && *cursor != ':' && material_length + 1 < entry.material.size())
            entry.material[material_length++] = *cursor++;
        if (*cursor++ != ':' || material_length == 0 || *cursor < '0' || *cursor > '9') return 0;
        temperature = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            temperature = temperature * 10 + unsigned(*cursor++ - '0');
            if (temperature > 999) return 0;
        }
        if (*cursor != ',' && *cursor != '\0') return 0;
        if (*cursor == ',') ++cursor;
        entry.material[material_length] = '\0';
        if (physical > 255 || logical > 255 || temperature > 32767) return 0;
        entry.physical_tool = physical;
        entry.logical_filament = logical;
        entry.temperature = temperature;
        ++count;
    }
    return *cursor == '\0' ? count : 0;
}

bool validate_batch(const std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> &entries, const size_t count) {
    uint8_t logical_mask = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto &entry = entries[i];
        if (entry.physical_tool >= EXTRUDERS || entry.logical_filament >= buddy::extrusion_calibration::max_logical_filaments
            || entry.temperature < thermalManager.extrude_min_temp || entry.temperature > HEATER_0_MAXTEMP - HEATER_MAXTEMP_SAFETY_MARGIN
            || (logical_mask & (1u << entry.logical_filament))) return false;
#if ENABLED(PRUSA_MMU2)
        if (entry.physical_tool != 0) return false;
#endif
        logical_mask |= 1u << entry.logical_filament;
        const auto &configured = config_store().get_filament_type(entry.logical_filament).parameters().name;
        if (strncmp(configured.data(), entry.material.data(), entry.material.size()) != 0) return false;
    }
    return count > 0;
}

bool run_batch(const std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> &entries, const size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const auto &entry = entries[i];
#if ENABLED(PRUSA_MMU2)
        M109_no_parser(PhysicalToolIndex::from_raw(0), { .target_temp = entry.temperature, .wait_heat = true, .wait_heat_or_cool = false });
        if (!MMU2::mmu2.tool_change_full(entry.logical_filament)) return false;
#else
        char tool_command[8];
        snprintf(tool_command, sizeof(tool_command), "T%u", entry.physical_tool);
        GcodeSuite::process_subcommands_now(tool_command);
        if (!stdext::holds_value(PhysicalToolIndex::currently_selected(), PhysicalToolIndex::from_raw(entry.physical_tool))) return false;
#endif
        char calibration_command[40];
        snprintf(calibration_command, sizeof(calibration_command), "M976 T%u L%u S%d", entry.physical_tool, entry.logical_filament, entry.temperature);
        GcodeSuite::process_subcommands_now(calibration_command);
        if (!buddy::extrusion_calibration::job_result(entry.logical_filament)) return false;
    }
    return true;
}
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
    mapi::park(mapi::get_parking_position(mapi::ParkPosition::purge));
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
    planner.buffer_line(pos, 8.0f, PhysicalToolIndex::currently_selected());
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
    mapi::home_if_needed_and_park(mapi::get_parking_position(mapi::ParkPosition::purge));
#else
    // The front service travel is outside the printable Y range on MK4,
    // CORE One and XL. Keep X aligned with the later cleanup slot so hanging
    // strands from different logical filaments remain separated.
    const mapi::ParkingPosition position {
        .x = 8.0f + slot * 11.0f,
        .y = Y_MIN_POS + 1.0f,
        .z = mapi::ParkingPosition::AtLeast { .absolute = 10.0f },
    };
    mapi::home_if_needed_and_park(position);
#endif
}
} // namespace

void PrusaGcodeSuite::M976() {
#if !(PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX())
    SERIAL_ERROR_MSG("M976 unsupported printer");
    return;
#else
    // S is a calibration-only target. Restore every hotend target on every exit path, including
    // batch/MMU failures and cached results. Nested M976 calls restore to the batch's temporary
    // target; the outer batch guard then restores the targets that existed before the command.
    HotendTargetRestorer restore_hotend_targets;
    if (parser.seen('A')) {
        std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> entries {};
        const size_t count = parse_batch_manifest(parser.command_ptr, entries);
        if (!validate_batch(entries, count)) {
            SERIAL_ERROR_MSG("M976 invalid batch or loaded-material mismatch");
            return;
        }
        if (!run_batch(entries, count)) {
            SERIAL_ERROR_MSG("M976 batch tool/MMU change or calibration failed");
            return;
        }
        SERIAL_ECHOLNPAIR("PA_CALIBRATION batch complete entries=", count);
        return;
    }
    const auto selected_tool = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::currently_selected());
    const uint8_t tool = parser.byteval('T', selected_tool.has_value() ? selected_tool->to_raw() : 0);
    const uint8_t slot = parser.byteval('L', tool);
    if (parser.boolval('C', false)) {
        if (slot >= buddy::extrusion_calibration::max_logical_filaments) {
            SERIAL_ERROR_MSG("M976 invalid anchor slot");
            return;
        }
        buddy::extrusion_calibration::clear_anchor(slot);
        SERIAL_ECHOLNPAIR("PA_CALIBRATION anchor slot cleared=", slot);
        return;
    }
    if (!selected_tool.has_value() || tool != selected_tool->to_raw() || slot >= buddy::extrusion_calibration::max_logical_filaments) {
        SERIAL_ERROR_MSG("M976 select requested tool/slot first");
        return;
    }
    if (parser.seenval('S')) {
        const int16_t target_temperature = static_cast<int16_t>(parser.value_celsius());
        if (target_temperature < thermalManager.extrude_min_temp) {
            SERIAL_ERROR_MSG("M976 temperature below extrusion minimum");
            return;
        }
        M109_no_parser(*selected_tool, {
            .target_temp = target_temperature,
            .wait_heat = true,
            .wait_heat_or_cool = false,
        });
    }
    if (const auto *cached = buddy::extrusion_calibration::job_result(slot)) {
        pressure_advance::set_axis_e_config({ cached->pressure_advance, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(slot, cached->max_flow_mm3_s);
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
    const float fallback = std::clamp(parser.floatval('B', buddy::extrusion_calibration::profile_pressure_advance_or(material_fallback(slot))), 0.0f, absolute_pa_max);
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
    for (int8_t offset = -3; offset <= 3; ++offset) {
        const float candidate = std::clamp(fallback + offset * 0.01f, 0.0f, absolute_pa_max);
        const auto score = run_bursts(candidate);
        if (score.valid && score.transient < best_cost) {
            best_cost = score.transient;
            best_pa = candidate;
            best_score = score;
        }
    }
    const float refinement_center = best_pa;
    for (int8_t offset = -2; offset <= 2; ++offset) {
        const float candidate = std::clamp(refinement_center + offset * 0.002f, 0.0f, absolute_pa_max);
        const auto score = run_bursts(candidate);
        if (score.valid && score.transient < best_cost) {
            best_cost = score.transient;
            best_pa = candidate;
            best_score = score;
        }
    }
    if (!std::isfinite(best_cost)) {
        pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(slot, material_flow_limit(slot));
        cleanup(slot, anchor_z);
        buddy::extrusion_calibration::suspend_pressure_monitor(false);
        SERIAL_ERROR_MSG("M976 invalid loadcell signal; fallback applied");
        return;
    }

    const float max_flow = material_flow_limit(slot);
    const buddy::extrusion_calibration::Result result { best_pa, max_flow, 1.0f / (1.0f + best_cost), true, best_score };
    buddy::extrusion_calibration::set_job_result(slot, result);
    pressure_advance::set_axis_e_config({ best_pa, pressure_advance::get_axis_e_config().smooth_time });
    planner.set_max_volumetric_flow(slot, max_flow);
    cleanup(slot, anchor_z);
    buddy::extrusion_calibration::configure_pressure_monitor(best_score, 0.8f, 8.0f);
    buddy::extrusion_calibration::suspend_pressure_monitor(false);
    SERIAL_ECHOLNPAIR("PA_CALIBRATION tool=", tool, " slot=", slot, " result=", best_pa, " max_flow=", max_flow, " confidence=", result.confidence);
#endif
}

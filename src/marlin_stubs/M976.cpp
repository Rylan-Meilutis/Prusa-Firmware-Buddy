#include "PrusaGcodeSuite.hpp"

#include <Marlin/src/feature/pressure_advance/pressure_advance.hpp>
#include <Marlin/src/feature/pressure_advance/pressure_advance_config.hpp>
#include <Marlin/src/feature/prusa/e-stall_detector.h>
#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/gcode/queue.h>
#include <Marlin/src/gcode/temperature/M104_M109.hpp>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/probe.h>
#include <Marlin/src/module/temperature.h>
#include <Marlin/src/module/tool_change.h>
#if ENABLED(PRUSA_MMU2)
#include <Marlin/src/feature/prusa/MMU2/mmu2_mk4.h>
#endif
#include <common/feature/extrusion_calibration.hpp>
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <common/mapi/motion.hpp>
#include <common/mapi/parking.hpp>
#include <common/marlin_server.hpp>
#include <common/marlin_server_types/client_response.hpp>
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
// Keep the search local to the slicer/profile fallback, while permitting
// legitimate high-PA Bowden and flexible-material profiles.
constexpr float absolute_pa_max = 0.50f;
struct BatchEntry {
    uint8_t physical_tool;
    uint8_t logical_filament;
    int16_t temperature;
    std::array<char, 17> material {};
};

class FilamentSensorEventGuard {
public:
    FilamentSensorEventGuard() { FSensors_instance().IncEvLock(); }
    ~FilamentSensorEventGuard() { FSensors_instance().DecEvLock(); }
};

class PressureMonitorGuard {
public:
    PressureMonitorGuard() { buddy::extrusion_calibration::suspend_pressure_monitor(true); }
    ~PressureMonitorGuard() { buddy::extrusion_calibration::suspend_pressure_monitor(false); }
};

class CalibrationCommandGuard {
public:
    CalibrationCommandGuard() { buddy::extrusion_calibration::set_calibration_command_active(true); }
    ~CalibrationCommandGuard() { buddy::extrusion_calibration::set_calibration_command_active(false); }
};

float probe_anchor_slot(uint8_t slot);

void clean_after_mmu_unload() {
#if ENABLED(PROBE_CLEANUP_SUPPORT)
    // MMU ramming can leave a short purge tail on the nozzle. Remove it on
    // the sacrificial front-right cleaning strip before any move crosses the
    // printable bed. The nozzle is already homed during PA batches, so the
    // cleanup helper's conditional home does not add an unsafe traverse.
    constexpr float cleaning_width = 32.0f;
    constexpr float right_margin = 10.0f;
    const xy_pos_t rect_max { X_MAX_POS - right_margin, Y_MIN_POS + 14.5f };
    const xy_pos_t rect_min { rect_max.x - cleaning_width, Y_MIN_POS + 10.5f };
    if (!cleanup_probe(rect_min, rect_max))
        SERIAL_ECHO_MSG("PA_CALIBRATION MMU post-unload nozzle cleaning incomplete");
#endif
}

void create_hotend_clearance() {
    // Keep the hot nozzle away from the sheet while it rises from the probing
    // temperature to the extrusion temperature. On moving-bed machines this
    // lowers the bed; on bedslingers it raises the tool by the same clearance.
    constexpr float heating_clearance = 10.0f;
    do_blocking_move_to_z(std::min(current_position.z + heating_clearance, float(Z_MAX_POS)), 5.0f);
}

void pa_fsm_change(const PhasesPressureAdvanceCalibration phase, const uint8_t progress, const uint8_t slot) {
    marlin_server::fsm_change(phase, { progress, static_cast<uint8_t>(slot + 1) });
}

bool pa_abort_requested(const PhasesPressureAdvanceCalibration phase) {
    return marlin_server::get_response_from_phase(phase) == Response::Abort;
}

void present_manual_result(const uint8_t tool, const uint8_t slot, const float pa) {
#if ENABLED(PRUSA_MMU2)
    if (MMU2::mmu2.Enabled()) MMU2::mmu2.unload();
#endif
    const uint16_t milli = static_cast<uint16_t>(std::clamp(lroundf(pa * 1000.0f), 0l, 65535l));
    marlin_server::fsm_change(PhasesPressureAdvanceCalibration::result,
        { 100, static_cast<uint8_t>(slot + 1), static_cast<uint8_t>(milli), static_cast<uint8_t>(milli >> 8) });
    if (marlin_server::wait_for_response(PhasesPressureAdvanceCalibration::result) == Response::Save) {
        if (FILE *file = fopen("/usb/pa-calibration.gcode", "a")) {
            fprintf(file, "M572 D%u S%.3f ; logical slot %u\n", unsigned(tool), static_cast<double>(pa), unsigned(slot));
            fclose(file);
        } else {
            SERIAL_ERROR_MSG("M976 could not save /usb/pa-calibration.gcode");
        }
    }
}

void present_manual_batch_results(const std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> &entries, const size_t count) {
#if ENABLED(PRUSA_MMU2)
    if (MMU2::mmu2.Enabled()) MMU2::mmu2.unload();
#endif
    uint8_t slot_mask = 0;
    for (size_t i = 0; i < count; ++i) slot_mask |= 1u << entries[i].logical_filament;
    marlin_server::fsm_change(PhasesPressureAdvanceCalibration::result, { 100, slot_mask, 0xff, 0xff });
    if (marlin_server::wait_for_response(PhasesPressureAdvanceCalibration::result) != Response::Save) return;
    if (FILE *file = fopen("/usb/pa-calibration.gcode", "a")) {
        for (size_t i = 0; i < count; ++i) {
            if (const auto *result = buddy::extrusion_calibration::job_result(entries[i].logical_filament))
                fprintf(file, "M572 D%u S%.3f ; logical slot %u\n", unsigned(entries[i].physical_tool), static_cast<double>(result->pressure_advance), unsigned(entries[i].logical_filament));
        }
        fclose(file);
    } else {
        SERIAL_ERROR_MSG("M976 could not save /usb/pa-calibration.gcode");
    }
}

void emit_pressure_advance_gcode(const uint8_t tool, const uint8_t slot, const float pa) {
    char command[80];
    snprintf(command, sizeof(command), "M572 D%u S%.3f ; auto PA logical slot %u",
        unsigned(tool), static_cast<double>(pa), unsigned(slot));
    SERIAL_ECHOLN(command);
}

void park_after_calibration() {
    if (all_axes_homed()) {
        mapi::park(mapi::ZAction::move_to_at_least, mapi::park_positions[mapi::ParkPosition::park]);
    }
}

class HotendTargetRestorer {
public:
    HotendTargetRestorer() {
        for (uint8_t hotend = 0; hotend < HOTENDS; ++hotend) {
            targets_[hotend] = Temperature::degTargetHotend(hotend);
        }
    }
    ~HotendTargetRestorer() {
        restore();
    }

    void restore(const bool wait_for_reachable_targets = false) {
        for (uint8_t hotend = 0; hotend < HOTENDS; ++hotend) {
            Temperature::setTargetHotend(targets_[hotend], hotend);
        }
        if (!wait_for_reachable_targets) return;
        for (uint8_t hotend = 0; hotend < HOTENDS; ++hotend) {
            if (targets_[hotend] < thermalManager.extrude_min_temp) continue;
            M109_no_parser(hotend, {
                .target_temp = targets_[hotend],
                .wait_heat = true,
                .wait_heat_or_cool = true,
            });
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

bool parse_manual_temperatures(const char *command, std::array<int, buddy::extrusion_calibration::max_logical_filaments> &temperatures) {
    const char *cursor = strstr(command, " U");
    if (!cursor) return true;
    cursor += 2;
    for (size_t index = 0; index < temperatures.size(); ++index) {
        unsigned value = 0;
        if (*cursor < '0' || *cursor > '9') return false;
        while (*cursor >= '0' && *cursor <= '9') {
            value = value * 10 + unsigned(*cursor++ - '0');
            if (value > 999) return false;
        }
        temperatures[index] = value;
        if (index + 1 < temperatures.size()) {
            if (*cursor++ != ',') return false;
        } else if (*cursor != '\0' && *cursor != ' ') {
            return false;
        }
    }
    return true;
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

bool run_batch(const std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> &entries, const size_t count, const bool manual) {
    // PA excitation and the deliberately short MMU load/unload moves are not
    // print-time filament failures. Suppress both sensor-event and loadcell
    // E-stall handling for the complete batch, including its tool changes.
    FilamentSensorEventGuard filament_sensor_events;
    BlockEStallDetection block_e_stall;
    PressureMonitorGuard pressure_monitor_guard;
    for (size_t i = 0; i < count; ++i) {
        const auto &entry = entries[i];
#if ENABLED(PRUSA_MMU2)
        float prepared_anchor_z = NAN;
        bool prepared_anchor = false;
        if (MMU2::mmu2.Enabled()) {
            // Probe while the filament path is empty. This prevents an MMU
            // load or hanging strand from contaminating the local micro-mesh.
            if (MMU2::mmu2.get_current_tool() != MMU2::FILAMENT_UNKNOWN) {
                if (!MMU2::mmu2.unload()) return false;
                clean_after_mmu_unload();
            }
            if (!GcodeSuite::G28_no_parser(true, true, true)) return false;
            prepared_anchor_z = probe_anchor_slot(entry.logical_filament);
            if (!HAS_WASTEBIN() && !std::isfinite(prepared_anchor_z)) return false;
            create_hotend_clearance();
            M109_no_parser(0, { .target_temp = entry.temperature, .wait_heat = true, .wait_heat_or_cool = false });
            if (!MMU2::mmu2.tool_change_for_pa_calibration(entry.logical_filament)) return false;
            prepared_anchor = true;
        } else if (entry.physical_tool != active_extruder || entry.logical_filament != active_extruder) {
            return false;
        }
#else
    CalibrationCommandGuard calibration_command_guard;
        char tool_command[8];
        snprintf(tool_command, sizeof(tool_command), "T%u", entry.physical_tool);
        GcodeSuite::process_subcommands_now(tool_command);
        if (active_extruder != entry.physical_tool) return false;
#endif
        char calibration_command[64];
#if ENABLED(PRUSA_MMU2)
        if (prepared_anchor) {
            snprintf(calibration_command, sizeof(calibration_command), "M976 %sT%u L%u S%d P Z%.3f", manual ? "M " : "", entry.physical_tool, entry.logical_filament, entry.temperature, static_cast<double>(prepared_anchor_z));
        } else {
            snprintf(calibration_command, sizeof(calibration_command), "M976 %sT%u L%u S%d", manual ? "M " : "", entry.physical_tool, entry.logical_filament, entry.temperature);
        }
#else
        snprintf(calibration_command, sizeof(calibration_command), "M976 %sT%u L%u S%d", manual ? "M " : "", entry.physical_tool, entry.logical_filament, entry.temperature);
#endif
        GcodeSuite::process_subcommands_now(calibration_command);
        if (!buddy::extrusion_calibration::job_result(entry.logical_filament)) return false;
    }
#if ENABLED(PRUSA_MMU2)
    // Leave the nozzle empty for the slicer's full MBL. Its normal initial
    // tool command reloads the print filament after probing is complete.
    if (MMU2::mmu2.Enabled() && MMU2::mmu2.get_current_tool() != MMU2::FILAMENT_UNKNOWN) {
        if (!MMU2::mmu2.unload()) return false;
        clean_after_mmu_unload();
    }
#endif
    // This is deliberately inside all calibration guards: after an MMU
    // unload, move away before the pressure monitor can observe normal motion.
    park_after_calibration();
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

float measure_idle_noise_floor() {
    auto &capture = buddy::extrusion_calibration::capture();
    planner.synchronize();
    capture.start();
    GcodeSuite::dwell(300);
    capture.stop();
    return capture.noise_floor();
}

float result_confidence(const buddy::extrusion_calibration::Score &score, const float idle_noise, const float separation = 1.0f) {
    if (!score.transitions_used || !std::isfinite(idle_noise)) return 0;
    const float effective_noise = std::max({ 0.25f, idle_noise, score.noise });
    const float snr = score.mean_load / effective_noise;
    const float signal_quality = std::clamp(snr / (snr + 2.0f), 0.0f, 1.0f);
    const float transition_quality = std::clamp(float(score.transitions_used) / std::max<float>(4.0f, score.transitions_detected), 0.0f, 1.0f);
    const float repeatability = std::isfinite(score.transient_stddev) ? 1.0f / (1.0f + score.transient_stddev) : 0.0f;
    const float sample_quality = std::clamp(score.sample_count / 128.0f, 0.0f, 1.0f);
    const auto evidence = [](const float value, const float weight) {
        return weight * std::log(std::max(value, 0.0001f));
    };
    float confidence = std::exp(evidence(signal_quality, 0.35f)
        + evidence(transition_quality, 0.20f)
        + evidence(repeatability, 0.15f)
        + evidence(sample_quality, 0.15f)
        + evidence(std::clamp(separation, 0.0f, 1.0f), 0.15f));
    if (score.capture_overflow) confidence *= 0.25f;
    return std::clamp(confidence, 0.0f, 1.0f);
}

float result_snr(const buddy::extrusion_calibration::Score &score, const float idle_noise) {
    if (!score.valid || !std::isfinite(idle_noise)) return 0;
    return score.mean_load / std::max({ 0.25f, idle_noise, score.noise });
}

void report_measurement_debug(const float candidate, const buddy::extrusion_calibration::Score &score, const float idle_noise) {
    char report[320];
    const float peak_snr = score.strongest_transition
        / std::max({ 0.25f, idle_noise, score.highest_transition_noise });
    snprintf(report, sizeof(report),
        "PA_CAL_DEBUG candidate=%.3f samples=%u transitions=%u used=%u low_signal=%u bad_timing=%u overflow=%u amplitude=%.2f transition_noise=%.2f idle_noise=%.2f peak_snr=%.2f snr=%.2f transient=%.3f repeat_sd=%.3f evidence=%.2f valid=%u",
        static_cast<double>(candidate), unsigned(score.sample_count), unsigned(score.transitions_detected),
        unsigned(score.transitions_used), unsigned(score.rejected_low_signal), unsigned(score.rejected_timing),
        unsigned(score.capture_overflow), static_cast<double>(score.strongest_transition),
        static_cast<double>(score.highest_transition_noise), static_cast<double>(idle_noise), static_cast<double>(peak_snr),
        static_cast<double>(result_snr(score, idle_noise)), static_cast<double>(score.transient), static_cast<double>(score.transient_stddev),
        static_cast<double>(result_confidence(score, idle_noise)), unsigned(score.valid));
    SERIAL_ECHOLN(report);
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
    const bool configure_confidence = parser.seen('Q');
    const bool configure_snr = parser.seen('N');
    const bool configure_retries = parser.seen('R');
    if (configure_confidence || configure_snr || configure_retries) {
        if (configure_confidence && parser.seenval('Q')) {
            const float requested = parser.value_float();
            if (requested < 0.50f || requested > 0.95f) {
                SERIAL_ERROR_MSG("M976 Q confidence must be 0.50..0.95");
                return;
            }
            config_store().pa_confidence_floor_percent.set(static_cast<uint8_t>(lroundf(requested * 100.0f)));
        }
        if (configure_snr && parser.seenval('N')) {
            const float requested = parser.value_float();
            if (requested < 3.0f || requested > 20.0f) {
                SERIAL_ERROR_MSG("M976 N minimum SNR must be 3.0..20.0");
                return;
            }
            config_store().pa_minimum_snr.set(requested);
        }
        if (configure_retries && parser.seenval('R')) {
            const int requested = parser.value_int();
            if (requested < 0 || requested > 10) {
                SERIAL_ERROR_MSG("M976 R retries must be 0..10");
                return;
            }
            config_store().pa_confidence_retries.set(static_cast<uint8_t>(requested));
        }
        SERIAL_ECHOLNPAIR("PA_CALIBRATION minimum_confidence=", config_store().pa_confidence_floor_percent.get() / 100.0f,
            " minimum_snr=", config_store().pa_minimum_snr.get(), " retries=", config_store().pa_confidence_retries.get());
        if (!parser.seen('A') && !parser.seen('K') && !parser.seen('T') && !parser.seen('L')) return;
    }
    // S is a calibration-only target. Restore every hotend target on every exit path, including
    // batch/MMU failures and cached results. Nested M976 calls restore to the batch's temporary
    // target; the outer batch guard then restores the targets that existed before the command.
    HotendTargetRestorer restore_hotend_targets;
    const bool manual = parser.boolval('M', false);
    if (parser.seen('A') || parser.seenval('K')) {
        std::array<BatchEntry, buddy::extrusion_calibration::max_logical_filaments> entries {};
        size_t count = 0;
        if (strstr(parser.command_ptr, " A")) {
            count = parse_batch_manifest(parser.command_ptr, entries);
        } else {
            const uint8_t mask = parser.byteval('K', 0);
            const int requested_temperature = parser.intval('S', 0);
            std::array<int, buddy::extrusion_calibration::max_logical_filaments> requested_temperatures {};
            if (!parse_manual_temperatures(parser.string_arg, requested_temperatures)) {
                SERIAL_ERROR_MSG("M976 invalid per-tool temperature list");
                return;
            }
            for (uint8_t logical = 0; logical < buddy::extrusion_calibration::max_logical_filaments; ++logical) {
                if (!(mask & (1u << logical))) continue;
                const auto filament = config_store().get_filament_type(logical);
                if (filament == FilamentType::none || count >= entries.size()) continue;
                auto &entry = entries[count++];
#if ENABLED(PRUSA_MMU2)
                entry.physical_tool = 0;
#else
                entry.physical_tool = logical;
#endif
                entry.logical_filament = logical;
                const auto &params = filament.parameters();
                const int profile_temperature = std::clamp<int>(params.nozzle_temperature, 170, 300);
                const int tool_temperature = requested_temperatures[logical] ?: requested_temperature;
                entry.temperature = tool_temperature
                    ? std::clamp(tool_temperature, std::max(170, profile_temperature - 15), std::min(300, profile_temperature + 15))
                    : profile_temperature;
                strncpy(entry.material.data(), params.name.data(), entry.material.size() - 1);
            }
        }
        if (!validate_batch(entries, count)) {
            SERIAL_ERROR_MSG("M976 invalid batch or loaded-material mismatch");
            return;
        }
        SERIAL_ECHOLNPAIR("PA_CALIBRATION batch accepted entries=", count);
        // Own the foreground dialog for the whole batch. Nested single-tool
        // M976 calls reuse this FSM, so tool changes, MMU loading, homing and
        // probing cannot briefly return control to the menu between tools.
        marlin_server::FSM_Holder pa_fsm { PhasesPressureAdvanceCalibration::heating, { 2, static_cast<uint8_t>(entries[0].logical_filament + 1) } };
        if (manual) {
            for (size_t i = 0; i < count; ++i) {
                buddy::extrusion_calibration::clear_anchor(entries[i].logical_filament);
            }
        }
        // The batch owns presentation of its aggregated result; nested tool
        // calibrations only measure and accumulate their job-scoped results.
        if (!run_batch(entries, count, false)) {
            SERIAL_ERROR_MSG("M976 batch tool/MMU change or calibration failed");
            return;
        }
        // MMU batches finish by unloading. Leave the nozzle clear of the
        // anchor before restoring (and potentially cooling to) the previous
        // target so a strand cannot settle back onto the calibration point.
        // Slicer-driven calibration returns at the exact pre-command targets,
        // including waiting for cooldown to the probing temperature selected
        // by start G-code before its following MBL.
        if (!manual) restore_hotend_targets.restore(true);
        if (manual) present_manual_batch_results(entries, count);
        if (manual) park_after_calibration();
        SERIAL_ECHOLNPAIR("PA_CALIBRATION batch complete entries=", count);
        return;
    }
    const uint8_t tool = parser.byteval('T', active_extruder);
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
    if (tool != active_extruder || slot >= buddy::extrusion_calibration::max_logical_filaments) {
        SERIAL_ERROR_MSG("M976 select requested tool/slot first");
        return;
    }
    const float fallback = constrain(parser.floatval('B', buddy::extrusion_calibration::profile_pressure_advance_or(material_fallback(slot))), 0.0f, absolute_pa_max);
    const float search_default = constrain(material_fallback(slot), 0.0f, absolute_pa_max);
    marlin_server::FSM_Holder pa_fsm { PhasesPressureAdvanceCalibration::heating, { 2, static_cast<uint8_t>(slot + 1) } };
    int16_t target_temperature = 0;
    if (parser.seenval('S')) {
        target_temperature = static_cast<int16_t>(parser.value_celsius());
        if (target_temperature < thermalManager.extrude_min_temp) {
            SERIAL_ERROR_MSG("M976 temperature below extrusion minimum");
            return;
        }
    }
    if (const auto *cached = buddy::extrusion_calibration::job_result(slot)) {
        pressure_advance::set_axis_e_config({ cached->pressure_advance, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(active_extruder, cached->max_flow_mm3_s);
        buddy::extrusion_calibration::configure_pressure_monitor(cached->pressure_reference, 0.8f, 8.0f);
        if (manual) present_manual_result(tool, slot, cached->pressure_advance);
        emit_pressure_advance_gcode(tool, slot, cached->pressure_advance);
        if (manual) park_after_calibration();
        SERIAL_ECHOLNPAIR("PA_CALIBRATION cached result=", cached->pressure_advance, " max_flow=", cached->max_flow_mm3_s);
        return;
    }
#if !HAS_WASTEBIN()
    if (buddy::extrusion_calibration::occupied_anchor_mask() & (1u << slot)) {
        SERIAL_ERROR_MSG("M976 anchor slot occupied; remove debris and run M976 C L<slot>");
        return;
    }
#endif
    // The deliberately pulsed extrusion profile is not a print-time runout or
    // autoload event. Keep sensor sampling active, but suppress event handling
    // until calibration cleanup is complete.
    FilamentSensorEventGuard filament_sensor_events;
    const bool prepared_anchor = parser.boolval('P', false);
    float anchor_z = prepared_anchor ? parser.floatval('Z', NAN) : NAN;
    if (!prepared_anchor) {
        if (target_temperature) {
            const int16_t probe_temperature = config_store().get_filament_type(slot).parameters().nozzle_preheat_temperature;
            M109_no_parser(tool, {
                .target_temp = probe_temperature,
                .wait_heat = true,
                .wait_heat_or_cool = true,
            });
        }
        pa_fsm_change(PhasesPressureAdvanceCalibration::homing, 8, slot);
        if (!GcodeSuite::G28_no_parser(true, true, true)) {
            SERIAL_ERROR_MSG("M976 homing failed");
            return;
        }
        if (pa_abort_requested(PhasesPressureAdvanceCalibration::homing)) {
            pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
            planner.set_max_volumetric_flow(active_extruder, material_flow_limit(slot));
            pa_fsm_change(PhasesPressureAdvanceCalibration::failed, 100, slot);
            return;
        }
        pa_fsm_change(PhasesPressureAdvanceCalibration::probing, 16, slot);
        anchor_z = probe_anchor_slot(slot);
        if (pa_abort_requested(PhasesPressureAdvanceCalibration::probing)) {
            pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
            planner.set_max_volumetric_flow(active_extruder, material_flow_limit(slot));
            pa_fsm_change(PhasesPressureAdvanceCalibration::failed, 100, slot);
            return;
        }
        create_hotend_clearance();
    }
    if (!HAS_WASTEBIN() && !std::isfinite(anchor_z)) {
        SERIAL_ERROR_MSG("M976 anchor micro-mesh failed");
        return;
    }
    if (target_temperature) {
        pa_fsm_change(PhasesPressureAdvanceCalibration::heating, 18, slot);
        M109_no_parser(tool, {
            .target_temp = target_temperature,
            .wait_heat = true,
            .wait_heat_or_cool = false,
        });
        if (pa_abort_requested(PhasesPressureAdvanceCalibration::heating)) {
            pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
            planner.set_max_volumetric_flow(active_extruder, material_flow_limit(slot));
            pa_fsm_change(PhasesPressureAdvanceCalibration::failed, 100, slot);
            return;
        }
    }
    if (thermalManager.tooColdToExtrude(active_extruder)) {
        SERIAL_ERROR_MSG("M976 hotend too cold");
        return;
    }
    park_for_free_air_calibration(slot);
    BlockEStallDetection block_legacy_e_stall;
    buddy::extrusion_calibration::suspend_pressure_monitor(true);
    auto high_precision = Loadcell::HighPrecisionEnabler(loadcell, !loadcell.IsHighPrecisionEnabled());
    loadcell.WaitBarrier();
    loadcell.Tare();

    const float idle_noise = measure_idle_noise_floor();
    const float minimum_result_confidence = constrain(config_store().pa_confidence_floor_percent.get() / 100.0f, 0.50f, 0.95f);
    const float minimum_snr = constrain(config_store().pa_minimum_snr.get(), 3.0f, 20.0f);
    const uint8_t maximum_confidence_retries = std::min<uint8_t>(config_store().pa_confidence_retries.get(), 10);
    float best_pa = search_default, best_cost = std::numeric_limits<float>::infinity();
    buddy::extrusion_calibration::Score best_score;
    struct CandidateObservation {
        float pa;
        float cost;
    };
    std::array<CandidateObservation, 32> observations {};
    size_t observation_count = 0;
    const auto record_observation = [&](const float candidate, const buddy::extrusion_calibration::Score &score) {
        if (score.valid && observation_count < observations.size()) {
            observations[observation_count++] = { candidate, score.transient };
        }
    };
    bool aborted = false;
    const auto measure = [&](const float candidate, const uint8_t progress) {
        pa_fsm_change(PhasesPressureAdvanceCalibration::measuring, progress, slot);
        if (pa_abort_requested(PhasesPressureAdvanceCalibration::measuring)) {
            aborted = true;
            return buddy::extrusion_calibration::Score {};
        }
        const float bounded_candidate = constrain(candidate, 0.0f, absolute_pa_max);
        const auto score = run_bursts(bounded_candidate);
        report_measurement_debug(bounded_candidate, score, idle_noise);
        record_observation(bounded_candidate, score);
        if (score.valid && score.transient < best_cost) {
            best_cost = score.transient;
            best_pa = bounded_candidate;
            best_score = score;
        }
        return score;
    };

    constexpr float initial_step = 0.02f;
    const auto center_seed = measure(search_default, 20);
    const auto lower_seed = measure(search_default - initial_step, 24);
    const auto upper_seed = measure(search_default + initial_step, 28);
    const float center_cost = center_seed.valid ? center_seed.transient : std::numeric_limits<float>::infinity();
    const float lower_cost = lower_seed.valid ? lower_seed.transient : std::numeric_limits<float>::infinity();
    const float upper_cost = upper_seed.valid ? upper_seed.transient : std::numeric_limits<float>::infinity();
    float lower, upper;
    if (lower_cost < center_cost && lower_cost <= upper_cost) {
        lower = std::max(0.0f, search_default - 0.03f);
        upper = search_default;
    } else if (upper_cost < center_cost) {
        lower = search_default;
        upper = std::min(absolute_pa_max, search_default + 0.03f);
    } else {
        lower = std::max(0.0f, search_default - initial_step);
        upper = std::min(absolute_pa_max, search_default + initial_step);
    }
    constexpr float golden_ratio = 0.61803398875f;
    float left = upper - golden_ratio * (upper - lower);
    float right = lower + golden_ratio * (upper - lower);
    auto left_score = measure(left, 32);
    auto right_score = measure(right, 36);
    for (uint8_t iteration = 0; iteration < 7 && !aborted && upper - lower > 0.0015f; ++iteration) {
        const float left_cost = left_score.valid ? left_score.transient : std::numeric_limits<float>::infinity();
        const float right_cost = right_score.valid ? right_score.transient : std::numeric_limits<float>::infinity();
        if (left_cost <= right_cost) {
            upper = right;
            right = left;
            right_score = left_score;
            left = upper - golden_ratio * (upper - lower);
            left_score = measure(left, static_cast<uint8_t>(40 + iteration * 4));
        } else {
            lower = left;
            left = right;
            left_score = right_score;
            right = lower + golden_ratio * (upper - lower);
            right_score = measure(right, static_cast<uint8_t>(40 + iteration * 4));
        }
    }

    const float verification_center = best_pa;
    measure(verification_center - 0.002f, 74);
    measure(verification_center, 78);
    measure(verification_center + 0.002f, 82);

    const auto selection_separation = [&]() {
        float competing_cost = std::numeric_limits<float>::infinity();
        for (size_t i = 0; i < observation_count; ++i) {
            if (std::abs(observations[i].pa - best_pa) > 0.001f) {
                competing_cost = std::min(competing_cost, observations[i].cost);
            }
        }
        if (!std::isfinite(best_cost) || !std::isfinite(competing_cost)) return 0.5f;
        const float relative_margin = std::max(0.0f, competing_cost - best_cost)
            / std::max(0.1f, std::abs(competing_cost));
        return 0.5f + 0.5f * std::tanh(relative_margin * 5.0f);
    };

    // The configured threshold is an acceptance floor, not a search target.
    // Refinement and neighbour verification above always finish first; only a
    // weak final signal receives bounded extra confirmations.
    for (uint8_t retry = 0; retry < maximum_confidence_retries && !aborted
         && (result_confidence(best_score, idle_noise, selection_separation()) < minimum_result_confidence || result_snr(best_score, idle_noise) < minimum_snr);
         ++retry) {
        pa_fsm_change(PhasesPressureAdvanceCalibration::measuring,
            static_cast<uint8_t>(82 + (retry * 5) / std::max<uint8_t>(maximum_confidence_retries, 1)), slot);
        if (pa_abort_requested(PhasesPressureAdvanceCalibration::measuring)) { aborted = true; break; }
        const auto score = run_bursts(best_pa);
        report_measurement_debug(best_pa, score, idle_noise);
        record_observation(best_pa, score);
        if (score.valid && score.transient < best_cost) {
            best_cost = score.transient;
            best_score = score;
        }
    }
    pa_fsm_change(PhasesPressureAdvanceCalibration::computing, 88, slot);
    if (pa_abort_requested(PhasesPressureAdvanceCalibration::computing)) aborted = true;
    if (aborted) {
        pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(active_extruder, material_flow_limit(slot));
        pa_fsm_change(PhasesPressureAdvanceCalibration::cleanup, 92, slot);
        cleanup(slot, anchor_z);
        buddy::extrusion_calibration::suspend_pressure_monitor(false);
        pa_fsm_change(PhasesPressureAdvanceCalibration::failed, 100, slot);
        SERIAL_ECHOLNPAIR("PA_CALIBRATION aborted fallback=", fallback);
        return;
    }
    const float separation_quality = selection_separation();
    const float confidence = result_confidence(best_score, idle_noise, separation_quality);
    SERIAL_ECHOLNPAIR("PA_CAL_SELECTION best=", best_pa, " cost=", best_cost,
        " separation=", separation_quality, " confidence=", confidence);
    if (!std::isfinite(best_cost) || confidence < minimum_result_confidence || result_snr(best_score, idle_noise) < minimum_snr) {
        const float max_flow = material_flow_limit(slot);
        pressure_advance::set_axis_e_config({ fallback, pressure_advance::get_axis_e_config().smooth_time });
        planner.set_max_volumetric_flow(active_extruder, max_flow);
        pa_fsm_change(PhasesPressureAdvanceCalibration::cleanup, 92, slot);
        cleanup(slot, anchor_z);
        buddy::extrusion_calibration::suspend_pressure_monitor(false);
        // A weak measurement is not a print failure. Publish the safe fallback
        // as this job's result so a batch continues and serial hosts do not
        // interpret the expected fallback path as a print-cancelling Error.
        buddy::extrusion_calibration::set_job_result(slot, { fallback, max_flow, confidence, true, best_score });
        if (manual) present_manual_result(tool, slot, fallback);
        else pa_fsm_change(PhasesPressureAdvanceCalibration::complete, 100, slot);
        emit_pressure_advance_gcode(tool, slot, fallback);
        char report[144];
        snprintf(report, sizeof(report), "PA_CALIBRATION tool=%u slot=%u fallback=%.3f confidence=%.2f reason=low_confidence",
            unsigned(tool), unsigned(slot), static_cast<double>(fallback), static_cast<double>(confidence));
        SERIAL_ECHOLN(report);
        return;
    }

    const float max_flow = material_flow_limit(slot);
    const buddy::extrusion_calibration::Result result { best_pa, max_flow, confidence, true, best_score };
    buddy::extrusion_calibration::set_job_result(slot, result);
    pressure_advance::set_axis_e_config({ best_pa, pressure_advance::get_axis_e_config().smooth_time });
    planner.set_max_volumetric_flow(active_extruder, max_flow);
    pa_fsm_change(PhasesPressureAdvanceCalibration::cleanup, 92, slot);
    cleanup(slot, anchor_z);
    buddy::extrusion_calibration::configure_pressure_monitor(best_score, 0.8f, 8.0f);
    buddy::extrusion_calibration::suspend_pressure_monitor(false);
    if (manual) present_manual_result(tool, slot, best_pa);
    else pa_fsm_change(PhasesPressureAdvanceCalibration::complete, 100, slot);
    emit_pressure_advance_gcode(tool, slot, best_pa);
    if (manual) park_after_calibration();
    char report[128];
    snprintf(report, sizeof(report), "PA_CALIBRATION tool=%u slot=%u result=%.3f max_flow=%.2f confidence=%.2f",
        unsigned(tool), unsigned(slot), static_cast<double>(best_pa), static_cast<double>(max_flow), static_cast<double>(result.confidence));
    SERIAL_ECHOLN(report);
#endif
}

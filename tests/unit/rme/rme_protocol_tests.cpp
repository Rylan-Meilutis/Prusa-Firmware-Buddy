#include <rme_protocol_parser.hpp>
#include <rme_file_transfer.hpp>
#include <rme_firmware_status.hpp>
#include <m976_material.hpp>
#include <m976_temperature_policy.hpp>
#include <m976_indx_policy.hpp>
#include <indx_serial_motion_safety.hpp>
#include <manual_motion_safety.hpp>

#include <filament_material.hpp>
#include <filament_material_family_storage.hpp>
#include <firmware_update_handoff.hpp>
#include <firmware_cleanup_gate.hpp>
#include <rme_light_hold.hpp>
#include <rme_light_mode.hpp>
#include <rme_host_progress.hpp>

#include <rme_active_tool.hpp>
#include <rme_spool_join.hpp>
#include <rme_indx_workflow.hpp>
#include <indx_dock_tolerance.hpp>
#include <g427_tool_selection.hpp>
#include <task_stack_requirements.hpp>
#include <unknown_axis_motion.hpp>
#include <host_keepalive_policy.hpp>
#include <m976_indx_pellet_policy.hpp>
#include <m976_extrusion_policy.hpp>
#include <serial_print_finalize_policy.hpp>
#include <marlin_server_types/marlin_server_state.h>

#if __has_include(<catch2/catch_test_macros.hpp>)
    #include <catch2/catch_test_macros.hpp>
#else
    #include <catch2/catch.hpp>
#endif

TEST_CASE("manual axes respect model ranges without trapping homed boundary positions") {
    using buddy::manual_motion_safety::axis_move_is_safe;
    CHECK(axis_move_is_safe(4, 100, { -1, 250 }));
    CHECK_FALSE(axis_move_is_safe(100, 260, { -1, 250 }));
    CHECK_FALSE(axis_move_is_safe(100, -2, { -1, 250 }));
    CHECK(axis_move_is_safe(-2, 4, { -1, 250 }));
    CHECK_FALSE(axis_move_is_safe(-2, -3, { -1, 250 }));
    CHECK(axis_move_is_safe(410, 200, { 0, 360 }));
    CHECK_FALSE(axis_move_is_safe(410, 420, { 0, 360 }));
    CHECK_FALSE(axis_move_is_safe(NAN, 100, { 0, 250 }));
    CHECK_FALSE(axis_move_is_safe(100, NAN, { 0, 250 }));
}

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

TEST_CASE("Chamber On and Locked respect the Active channel profile") {
    for (int8_t mode = 0; mode <= 2; ++mode) {
        CHECK(rme_light_mode::active_profile_brightness(mode, false, 255) == 0);
        CHECK(rme_light_mode::active_profile_brightness(mode, true, 0) == 0);
        CHECK(rme_light_mode::active_profile_brightness(mode, true, 73) == (mode ? 73 : 0));
    }
}

TEST_CASE("Print chamber override is binary and independent of idle activity", "[rme][light]") {
    rme_light_mode::PrintState print;
    rme_light_mode::State idle;
    CHECK(print.brightness(true, 192) == 192);
    CHECK(print.brightness(false, 192) == 0);
    print.set(0);
    idle.set(0, 100);
    idle.resume_on_activity();
    idle.apply_door_hold(true);
    idle.expire(50000, 30);
    CHECK(print.brightness(true, 192) == 0);
    print.set(2);
    CHECK(print.mode == 1);
    CHECK(print.brightness(true, 192) == 192);
    CHECK(print.brightness(false, 0) == 0);
    CHECK(print.brightness(false, 192) == 0);
    CHECK(print.brightness(true, 0) == 0);
    print.reset();
    CHECK(print.brightness(false, 192) == 0);
}

TEST_CASE("Print light toggling restores each channel profile without enabling dark channels", "[rme][light]") {
    rme_light_mode::PrintState print;
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (const bool enabled : { false, true }) {
            for (const uint8_t brightness : { 0, 73, 192, 255 }) {
                print.set(0);
                CHECK(print.brightness(enabled, brightness) == 0);
                print.set(1);
                CHECK(print.brightness(enabled, brightness) == (enabled ? brightness : 0));
            }
        }
    }
}

TEST_CASE("RME host progress preserves unknown and paused estimates with bounded freshness", "[rme][progress]") {
    rme_host_progress::State state;
    CHECK_FALSE(state.valid(0));
    state.set(24, 120, false, 0);
    CHECK(state.valid(0));
    CHECK(state.percent == 24);
    CHECK(state.seconds(5000) == 115);
    CHECK_FALSE(state.valid(60001));
    state.set(25, 120, true, 5000);
    CHECK(state.seconds(55000) == 120);
    state.set(25, state.unknown, false, 5000);
    CHECK(state.seconds(55000) == state.unknown);
    state.set(25, 1, false, UINT32_MAX - 1000);
    CHECK(state.valid(1000));
    CHECK(state.seconds(1000) == 0);
    state = {};
    CHECK_FALSE(state.valid(1000));
}

TEST_CASE("Shared chamber mode expires once and survives clock rollover", "[rme][light]") {
    rme_light_mode::State state;
    CHECK(state.mode == -1);
    CHECK_FALSE(state.set(3, 0));
    REQUIRE(state.set(1, UINT32_MAX - 499));
    CHECK_FALSE(state.expire(499, 1));
    CHECK(state.expire(500, 1));
    CHECK(state.mode == 0);
    CHECK_FALSE(state.expire(501, 1));
    REQUIRE(state.set(2, 0));
    CHECK_FALSE(state.expire(UINT32_MAX, 0));
    CHECK(state.mode == 2);
    for (uint32_t i = 0; i < 10000; ++i) {
        REQUIRE(state.set(1, i * 2000));
        REQUIRE(state.expire(i * 2000 + 1000, 1));
    }
}

TEST_CASE("Chamber mode transitions restart shared screen and status idle countdown", "[rme][light][regression]") {
    rme_light_mode::State mode;
    rme_light_hold::State hold;
    uint32_t timestamp = 0;
    REQUIRE(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    REQUIRE(mode.set(0, 1234));
    rme_light_mode::restart_idle_countdown(hold, timestamp, 1234);
    CHECK_FALSE(hold.active());
    CHECK(hold.consume_automatic_release());
    CHECK(timestamp == 1234);

    REQUIRE(mode.set(1, 2000));
    REQUIRE(mode.expire(3000, 1));
    rme_light_mode::restart_idle_countdown(hold, timestamp, 3000);
    CHECK(timestamp == 3000);
    CHECK_FALSE(mode.expire(4000, 1));
    CHECK(timestamp == 3000); // Subsequent polls must not extend the timer.

    rme_light_mode::restart_idle_countdown(hold, timestamp, 0);
    CHECK(timestamp != 0);
    CHECK(uint32_t(0 - timestamp) == 1);
}

TEST_CASE("Door and local activity resume automatic lighting after slider Off", "[rme][light][regression]") {
    rme_light_mode::State state;
    REQUIRE(state.set(0, 100));
    CHECK(state.resume_on_activity());
    CHECK(state.mode == -1);
    CHECK_FALSE(state.resume_on_activity());
    REQUIRE(state.set(1, 100));
    REQUIRE(state.expire(1100, 1));
    CHECK(state.resume_on_activity());
    CHECK(state.mode == -1);
    REQUIRE(state.set(2, 100));
    CHECK_FALSE(state.resume_on_activity());
    CHECK(state.mode == 2);
    REQUIRE(state.set(0, 100));
    CHECK_FALSE(state.expire(10000, 1));
    CHECK(state.mode == 0); // Timer updates/polling are not activity.
}

TEST_CASE("Streamed print moves do not cancel RME lighting Off", "[rme][light][regression]") {
    rme_light_mode::State state;
    REQUIRE(state.set(0, 100));
    for (int move = 0; move < 1000; ++move) {
        if (rme_light_mode::serial_commands_wake_lights(true)) {
            state.resume_on_activity();
        }
    }
    CHECK(state.mode == 0);
    CHECK_FALSE(rme_light_mode::serial_commands_wake_lights(true));
    REQUIRE(rme_light_mode::serial_commands_wake_lights(false));
    CHECK(state.resume_on_activity()); // Idle jog wakes the light.
    REQUIRE(state.set(0, 200));
    CHECK(state.apply_door_hold(true)); // Door behavior is unchanged.
    CHECK(state.mode == -1);
}

TEST_CASE("An already open door overrides temporary RME light modes without restarting idle", "[rme][light][regression]") {
    rme_light_mode::State mode;
    rme_light_hold::State hold;
    uint32_t timestamp = 100;
    for (const uint8_t requested : { 0, 1, 0, 1 }) {
        REQUIRE(mode.set(requested, 200));
        CHECK(mode.apply_door_hold(true));
        rme_light_mode::restart_idle_countdown(hold, timestamp, 200, true);
        CHECK(mode.mode == -1); // Report/use the normal active-state profile.
        CHECK(timestamp == 100);
        CHECK_FALSE(mode.expire(10000, 1));
        CHECK_FALSE(mode.apply_door_hold(true)); // Stable sensor polls.
    }
    // Closing the door resumes the normal countdown from closing time.
    CHECK_FALSE(mode.apply_door_hold(false));
    rme_light_mode::restart_idle_countdown(hold, timestamp, 11000, false);
    CHECK(timestamp == 11000);
    REQUIRE(mode.set(1, 11000));
    CHECK_FALSE(mode.apply_door_hold(false));
    CHECK(mode.expire(12000, 1));
}

TEST_CASE("Door lighting policy retains Locked and respects disabled door hold", "[rme][light]") {
    rme_light_mode::State mode;
    REQUIRE(mode.set(2, 100));
    CHECK_FALSE(mode.apply_door_hold(true));
    CHECK(mode.mode == 2);
    CHECK_FALSE(mode.expire(10000, 1));
    REQUIRE(mode.set(0, 100));
    CHECK_FALSE(mode.apply_door_hold(false));
    CHECK(mode.mode == 0);
    REQUIRE(mode.set(1, 100));
    CHECK_FALSE(mode.apply_door_hold(false));
    CHECK(mode.expire(1100, 1));
}

TEST_CASE("Host keepalive remains active when calibration suspends auto reports", "[rme][serial][regression]") {
    using buddy::host_keepalive_policy::should_emit;

    // Auto-report state is deliberately absent from this policy. M97x/G426 and
    // similar long operations may suppress telemetry, never the host heartbeat.
    CHECK(should_emit(2, true));
    CHECK_FALSE(should_emit(0, true));
    CHECK_FALSE(should_emit(2, false));
}

TEST_CASE("Media completion marker is never emitted for a serial job", "[rme][serial][regression]") {
    using buddy::serial_print_finalize_policy::emit_file_printed_marker;

    CHECK(emit_file_printed_marker(false));
    CHECK_FALSE(emit_file_printed_marker(true));
}

TEST_CASE("Serial result screens survive success and cancellation", "[rme][serial][regression]") {
    using namespace buddy::serial_print_finalize_policy;
    using marlin_server::State;
    for (const auto state : { State::Finished, State::Aborted }) {
        CHECK(is_result(state));
        CHECK(keep_result_screen(true, state));
        CHECK_FALSE(keep_result_screen(false, state));
    }
    for (const auto state : { State::Printing, State::Paused, State::Aborting_WaitIdle, State::Idle }) {
        CHECK_FALSE(is_result(state));
        CHECK_FALSE(keep_result_screen(true, state));
    }
}

TEST_CASE("INDX PA bounds pellet buildup without ejecting every cycle", "[rme][m976][indx][pellet]") {
    using namespace buddy::m976_indx_pellet_policy;
    uint8_t pending = 0;
    for (uint8_t cycle = 1; cycle < cycles_per_ejection; ++cycle) {
        CHECK_FALSE(record_cycle_and_should_eject(pending));
        CHECK(pending == cycle);
    }
    CHECK(record_cycle_and_should_eject(pending));
    CHECK(final_ejection_needed(pending));
    pending = 0; // successful periodic wipe/ejection
    CHECK_FALSE(final_ejection_needed(pending));
    CHECK_FALSE(record_cycle_and_should_eject(pending));
    CHECK(final_ejection_needed(pending)); // final remainder is always ejected
}

TEST_CASE("INDX PA lets a wiped pellet solidify before ejection", "[rme][m976][indx][pellet]") {
    CHECK(buddy::m976_indx_pellet_policy::cooling_delay_ms == 15000);
    CHECK(buddy::m976_indx_pellet_policy::main_wiper_passes == 2);
}

TEST_CASE("Every PA material keeps five cycles and cools for fifteen seconds", "[rme][m976][indx][pellet]") {
    using namespace buddy::m976_indx_pellet_policy;
    uint8_t pending = 0;
    for (int i = 0; i < 4; ++i) {
        CHECK_FALSE(record_cycle_and_should_eject(pending));
    }
    CHECK(record_cycle_and_should_eject(pending));
    CHECK(pending == 5);
    CHECK(final_ejection_needed(pending));
    for (const auto material : { "PETG", "PCTG", "ASA", "ABS", "PC", "PA", "PPA", "HIPS" }) {
        CHECK(cooling_delay_for(material, 220) == 15000);
    }
    CHECK(cooling_delay_for("custom", 230) == 15000);
    CHECK(cooling_delay_for("PLA", 215) == 15000);
}

TEST_CASE("Filtration leaves active job mode on both terminal result screens") {
    using namespace marlin_server;
    for (const auto state : { State::Printing, State::Paused, State::Aborting_Begin,
             State::Aborting_WaitIdle, State::Aborting_UnloadFilament, State::Aborting_ParkHead,
             State::Finishing_WaitIdle, State::Finishing_UnloadFilament, State::Finishing_ParkHead }) {
        CHECK(is_filtration_job_active(state));
    }
    for (const auto state : { State::Aborted, State::Finished, State::Exit, State::Idle }) {
        CHECK_FALSE(is_filtration_job_active(state));
    }
}

TEST_CASE("RME exposes every INDX nozzle mismatch phase as a stable workflow", "[rme][indx]") {
    using rme_indx_workflow::nozzle_mismatch;
    CHECK(std::string_view(nozzle_mismatch(0).workflow) == "indx_tool_detection"sv);
    CHECK(std::string_view(nozzle_mismatch(0).phase) == "unknown_tool_detected"sv);
    CHECK(nozzle_mismatch(0).waiting_for_host);
    CHECK(std::string_view(nozzle_mismatch(1).workflow) == "indx_slot_selection"sv);
    CHECK(std::string_view(nozzle_mismatch(3).phase) == "dock_not_empty"sv);
    CHECK(nozzle_mismatch(3).waiting_for_host);
    CHECK(std::string_view(nozzle_mismatch(4).phase) == "tool_lost"sv);
    CHECK(std::string_view(nozzle_mismatch(6).phase) == "pickup_failed"sv);
    CHECK(std::string_view(nozzle_mismatch(7).phase) == "park_failed"sv);
    CHECK(std::string_view(nozzle_mismatch(8).phase) == "confirm_abort"sv);
    CHECK(std::string_view(nozzle_mismatch(255).workflow) == "indx"sv);
}

TEST_CASE("RME names every INDX calibration phase", "[rme][indx]") {
    using namespace rme_indx_workflow;
    CHECK(std::string_view(dock_calibration_phase(3)) == "select_docks"sv);
    CHECK(std::string_view(dock_calibration_phase(11)) == "validating"sv);
    CHECK(std::string_view(dock_calibration_phase(14)) == "failed"sv);
    CHECK(std::string_view(nozzle_cleaner_phase(8)) == "measuring_x"sv);
    CHECK(std::string_view(nozzle_cleaner_phase(14)) == "evaluating_y"sv);
    CHECK(std::string_view(tool_offsets_phase(1, true, true)) == "clean_nozzles_cold"sv);
    CHECK(std::string_view(tool_offsets_phase(2, true, true)) == "clean_nozzles_hot"sv);
    CHECK(std::string_view(tool_offsets_phase(4, true, true)) == "homing"sv);
    CHECK(std::string_view(tool_offsets_phase(5, true, true)) == "picking_tool"sv);
    CHECK(std::string_view(tool_offsets_phase(6, true, true)) == "calibrating"sv);
    CHECK(std::string_view(tool_offsets_phase(8, true, true)) == "failed"sv);
    CHECK(std::string_view(tool_offsets_phase(1, false, false)) == "ensure_nozzles_clean"sv);
    CHECK(std::string_view(tool_offsets_phase(3, false, false)) == "picking_tool"sv);
    CHECK(std::string_view(tool_offsets_phase(4, false, false)) == "homing"sv);
    CHECK(std::string_view(tool_offsets_phase(5, false, false)) == "calibrating"sv);
    CHECK(std::string_view(tool_offsets_phase(7, false, false)) == "failed"sv);
    CHECK(std::string_view(tool_offsets_phase(8, false, false)) == "unknown"sv);
}

TEST_CASE("RME active tool reports a parked toolchanger as none", "[rme][tool][regression]") {
    struct TestTool {
        uint8_t value;
        uint8_t to_raw() const { return value; }
    };
    struct TestNoTool {};

    const auto parked = rme_active_tool::snapshot(std::variant<TestTool, TestNoTool> { TestNoTool {} });
    CHECK_FALSE(parked.selected);

    const auto selected = rme_active_tool::snapshot(
        std::variant<TestTool, TestNoTool> { TestTool { 2 } });
    CHECK(selected.selected);
    CHECK(selected.index == 2);
}

TEST_CASE("RME spool join endpoints are distinct and bounded", "[rme][spooljoin]") {
    using rme_spool_join::valid_pair;

    CHECK(valid_pair(0, 1, 5));
    CHECK(valid_pair(3, 4, 5));
    CHECK_FALSE(valid_pair(1, 1, 5));
    CHECK_FALSE(valid_pair(-1, 1, 5));
    CHECK_FALSE(valid_pair(0, -1, 5));
    CHECK_FALSE(valid_pair(5, 0, 5));
    CHECK_FALSE(valid_pair(0, 5, 5));
}

TEST_CASE("RME Marlin task stack retains crash-derived guard space", "[rme][stack][regression]") {
    using namespace buddy::task_stack_requirements;

    CHECK(marlin_words == 1664);
    CHECK(marlin_guard_bytes >= required_guard_bytes);
    CHECK(marlin_guard_bytes >= observed_rme_overrun_bytes * 4);
}

TEST_CASE("RME firmware status accepts only matching verified metadata", "[rme][firmware][regression]") {
    rme_firmware_status::VerifiedMetadata metadata {};
    metadata.size = 4'000'000;
    metadata.sha256[0] = 0x42;

    CHECK(rme_firmware_status::valid(metadata, 4'000'000));
    CHECK_FALSE(rme_firmware_status::valid(metadata, 3'999'999));
    metadata.magic = 0;
    CHECK_FALSE(rme_firmware_status::valid(metadata, 4'000'000));
    metadata.magic = rme_firmware_status::verified_metadata_magic;
    metadata.version++;
    CHECK_FALSE(rme_firmware_status::valid(metadata, 4'000'000));
}

TEST_CASE("RME firmware query deadlines are wrap-safe", "[rme][firmware][regression]") {
    CHECK_FALSE(rme_firmware_status::elapsed(999, 0, 1'000));
    CHECK(rme_firmware_status::elapsed(1'000, 0, 1'000));
    CHECK(rme_firmware_status::elapsed(5, UINT32_MAX - 5, 10));
    CHECK_FALSE(rme_firmware_status::elapsed(5, UINT32_MAX - 5, 12));
}

TEST_CASE("RME firmware validation rejects truncated and failed reads", "[rme][firmware][regression]") {
    CHECK(rme_firmware_status::complete_read_valid(4096, 4096, true, false));
    CHECK_FALSE(rme_firmware_status::complete_read_valid(2048, 4096, true, false));
    CHECK_FALSE(rme_firmware_status::complete_read_valid(4096, 4096, false, false));
    CHECK_FALSE(rme_firmware_status::complete_read_valid(4096, 4096, true, true));
}

TEST_CASE("M976 validates material family independently of custom profile name", "[rme][m976][regression]") {
    using buddy::m976_material::fallback;
    using buddy::m976_material::matches;

    CHECK(matches("PETG"sv, "PET-00L"sv, "PETG"sv));
    CHECK_FALSE(matches("PLA"sv, "PET-00L"sv, "PETG"sv));
    CHECK(matches("PETG"sv, "PETG"sv, "PETG"sv));
    CHECK(matches("PET-00L"sv, "PET-00L"sv, "PETG"sv));
    CHECK(fallback("PET-00L"sv, "PETG"sv) == 0.045f);
    CHECK(matches("PETG-CF"sv, "PETG-CF"sv, {}));
    CHECK_FALSE(matches("PETG"sv, "PETG-CF"sv, {}));
    CHECK(fallback("PETG-CF"sv, {}) == 0.045f);
    for (const auto requested : { "TPU"sv, "TPE"sv, "FLEX"sv, "TPU-95A"sv }) {
        for (const auto base : { "TPU"sv, "TPE"sv, "FLEX"sv }) {
            CHECK(matches(requested, "TPU-00L"sv, base));
            CHECK(fallback("custom"sv, base) == 0.08f);
        }
        CHECK_FALSE(matches(requested, "PLA-001"sv, "PLA"sv));
        CHECK_FALSE(matches(requested, "TPU-00L"sv, "PETG"sv));
    }
    CHECK(matches("FLEX"sv, "TPE-001"sv, {}));
    CHECK(matches("TPU-00L"sv, "TPU-00L"sv, "FLEX"sv));
    CHECK_FALSE(matches("PLA"sv, "TPU-00L"sv, "FLEX"sv));
}

TEST_CASE("M976 MMU service temperature stays above cold extrusion cutoff", "[rme][m976][temperature][regression]") {
    using namespace buddy::m976_temperature_policy;

    CHECK(probe_temperature(170) == 175);
    CHECK(probe_temperature(170) > 170);
    CHECK(calibration_temperature_ready(250.0f, 255, 170));
    CHECK(calibration_temperature_ready(255.0f, 255, 170));
    CHECK_FALSE(calibration_temperature_ready(249.9f, 255, 170));
    CHECK_FALSE(calibration_temperature_ready(169.9f, 170, 170));
    CHECK_FALSE(calibration_temperature_ready(255.0f, 160, 170));
    CHECK_FALSE(wait_for_restored_target);
}

TEST_CASE("M976 rejects stale or unsafe INDX manifest mappings before motion", "[rme][m976][indx][regression]") {
    using buddy::m976_indx_policy::safe_manifest_mapping;

    CHECK(safe_manifest_mapping(0, 0, 0, true, true));
    CHECK(safe_manifest_mapping(7, 7, 7, true, true));
    CHECK(safe_manifest_mapping(7, 0, 7, true, true));
    CHECK_FALSE(safe_manifest_mapping(7, 0, 0, true, true));
    CHECK_FALSE(safe_manifest_mapping(0, 7, 7, true, true));
    CHECK_FALSE(safe_manifest_mapping(7, 7, 7, false, true));
    CHECK_FALSE(safe_manifest_mapping(7, 7, 7, true, false));
}

TEST_CASE("M976 never uses legacy sheet-contact cleanup on INDX", "[rme][m976][indx][keepout][regression]") {
    using buddy::m976_indx_policy::uses_sheet_contact_cleanup;

    CHECK_FALSE(uses_sheet_contact_cleanup(true, true));
    CHECK_FALSE(uses_sheet_contact_cleanup(true, false));
    CHECK(uses_sheet_contact_cleanup(false, true));
    CHECK_FALSE(uses_sheet_contact_cleanup(false, false));
}

TEST_CASE("INDX serial motion protects service hardware without blocking front Y priming", "[rme][indx][keepout][motion]") {
    using namespace buddy::indx_serial_motion_safety;
    constexpr ServiceBoundary boundary { 250, 0 };
    STATIC_REQUIRE(point_is_safe(125, 100, boundary));
    STATIC_REQUIRE(point_is_safe(-20, 220, boundary)); // ordinary endstops handle non-service sides
    STATIC_REQUIRE_FALSE(point_is_safe(251, 100, boundary));
    STATIC_REQUIRE_FALSE(point_is_safe(125, -0.1f, boundary));
    STATIC_REQUIRE(linear_move_is_safe(125, 0, 125, -1.5f, false, boundary));
    STATIC_REQUIRE(linear_move_is_safe(125, -1.5f, 125, 0, false, boundary));
    // Tool and mesh transforms may make a Y-only command appear to shift X.
    STATIC_REQUIRE(linear_move_is_safe(88.10f, 73, 88.08f, 71.5f, false, boundary));
    STATIC_REQUIRE_FALSE(linear_move_is_safe(125, -1.5f, 126, -1.5f, true, boundary));
    STATIC_REQUIRE_FALSE(linear_move_is_safe(249, 100, 251, 100, true, boundary));
    STATIC_REQUIRE_FALSE(linear_move_is_safe(251, 100, 251, 98.5f, false, boundary));
    STATIC_REQUIRE(arc_is_safe(125, 100, 20, boundary));
    STATIC_REQUIRE_FALSE(arc_is_safe(245, 100, 10, boundary));
}

TEST_CASE("INDX arcs check the travelled sweep including full circles", "[rme][indx][keepout][regression]") {
    using namespace buddy::indx_serial_motion_safety;
    constexpr ServiceBoundary b { 250, 0 };
    // Exact N577 -> N578 from the failed first layer.
    CHECK(arc_sweep_is_safe(114.636f, 99.869f, 114.644f, 101.012f,
        114.636f - 177.041f, 99.869f + 1.855f, false, false, b));
    CHECK_FALSE(arc_sweep_is_safe(114.636f, 99.869f, 114.644f, 101.012f,
        114.636f - 177.041f, 99.869f + 1.855f, true, false, b));
    CHECK(arc_sweep_is_safe(245, 90, 245, 110, 245, 100, true, false, b));
    CHECK_FALSE(arc_sweep_is_safe(245, 90, 245, 110, 245, 100, false, false, b));
    CHECK_FALSE(arc_sweep_is_safe(245, 90, 245, 90, 245, 100, false, true, b));
    CHECK(arc_sweep_is_safe(125, 90, 125, 90, 125, 100, true, true, b));
    CHECK_FALSE(arc_sweep_is_safe(110, 5, 90, 5, 100, 5, true, false, b));
    CHECK(arc_sweep_is_safe(110, 5, 90, 5, 100, 5, false, false, b));
    CHECK_FALSE(arc_sweep_is_safe(251, 90, 245, 110, 245, 100, true, false, b));
    CHECK_FALSE(arc_sweep_is_safe(125, 100, 125, 100, 125, 100, false, true, b));
    CHECK_FALSE(arc_sweep_is_safe(NAN, 100, 125, 100, 125, 90, false, false, b));
    // Independent dense sampling: accepted arcs must not cross either boundary.
    for (bool cw : { false, true }) {
        for (int start = 0; start < 360; start += 15) {
            for (int travel = 15; travel < 360; travel += 15) {
                constexpr float rad = 0.017453292519943295f;
                const float end = start + (cw ? -travel : travel);
                const float x = 245 + 10 * std::cos(start * rad);
                const float y = 5 + 10 * std::sin(start * rad);
                const float tx = 245 + 10 * std::cos(end * rad);
                const float ty = 5 + 10 * std::sin(end * rad);
                if (arc_sweep_is_safe(x, y, tx, ty, 245, 5, cw, false, b)) {
                    for (int step = 0; step <= travel; ++step) {
                        const float angle = (start + (cw ? -step : step)) * rad;
                        CHECK(245 + 10 * std::cos(angle) <= 250.0001f);
                        CHECK(5 + 10 * std::sin(angle) >= -0.0001f);
                    }
                }
            }
        }
    }
}

TEST_CASE("G12 eject followed by Orca relative purge stays in calibrated cleaner lane", "[rme][indx][keepout][regression]") {
    using buddy::indx_serial_motion_safety::cleaner_purge_move_is_safe;
    CHECK(cleaner_purge_move_is_safe(0, 87, 0, 85.5f, false, false));
    CHECK(cleaner_purge_move_is_safe(0, 85.5f, 0, 87, false, false));
    CHECK_FALSE(cleaner_purge_move_is_safe(-12, 87, 0, 87, true, false));
    CHECK_FALSE(cleaner_purge_move_is_safe(0, 87, 0, 85.5f, false, true));
    CHECK_FALSE(cleaner_purge_move_is_safe(0, 87, 0, 75.9f, false, false));
    CHECK_FALSE(cleaner_purge_move_is_safe(0, 87, 0, 101.6f, false, false));
    CHECK_FALSE(cleaner_purge_move_is_safe(0.6f, 87, 0.6f, 85.5f, false, false));
    CHECK_FALSE(cleaner_purge_move_is_safe(0, 87, 0, 85.5f, true, false));
}

TEST_CASE("External filament material never exposes the custom profile name", "[rme][filament][regression]") {
    using buddy::filament_material::authoritative_name;

    CHECK(authoritative_name("PLA-00D", "PLA") == "PLA");
    CHECK(authoritative_name("PET-00L", "PETG") == "PETG");
    CHECK(authoritative_name("PA-CF", {}) == "PA-CF");
}

TEST_CASE("New base presets preserve the deployed EEPROM1 layout", "[rme][filament][eeprom][boot]") {
    using buddy::filament_material_family_storage::decode;
    using buddy::filament_material_family_storage::encode;
    constexpr uint8_t preset_count = 10;

    CHECK_FALSE(decode(0, preset_count).has_value());
    for (uint8_t preset = 0; preset < preset_count; ++preset) {
        const uint8_t persisted = encode(preset);
        REQUIRE(decode(persisted, preset_count).has_value());
        CHECK(*decode(persisted, preset_count) == preset);
    }

    // Old firmware always wrote zero. Any otherwise invalid spare-bit value
    // must fail closed instead of indexing the preset table during startup.
    CHECK_FALSE(decode(0, preset_count).has_value());
    CHECK_FALSE(decode(31, preset_count).has_value());
}

TEST_CASE("RME action names are complete tokens", "[rme][regression]") {
    CHECK(rme_protocol::action_is("ABORT"sv, "ABORT"sv));
    CHECK(rme_protocol::action_is("READ offset=0"sv, "READ"sv));
    CHECK_FALSE(rme_protocol::action_is("ABORTgarbage"sv, "ABORT"sv));
    CHECK_FALSE(rme_protocol::action_is("READ_BINARY"sv, "READ"sv));
    CHECK_FALSE(rme_protocol::action_is("READ\toffset=0"sv, "READ"sv));
}

TEST_CASE("RME parameters are token bounded", "[rme]") {
    constexpr auto command = "@RME FILAMENT SET slot=7 name=NEW nozzle=215 visible=1 tx=3933926906*42"sv;
    CHECK(rme_protocol::value(command, "slot") == "7");
    CHECK(rme_protocol::value(command, "name") == "NEW");
    CHECK(rme_protocol::value(command, "visible") == "1");
    CHECK_FALSE(rme_protocol::value(command, "set"));
    CHECK_FALSE(rme_protocol::value(command, "missing"));

    // A key embedded in another token must never alias the requested key.
    CHECK_FALSE(rme_protocol::value("@RME X nottx=9"sv, "tx"));
    CHECK(rme_protocol::value("@RME X tx=9 extra=1"sv, "tx") == "9");
}

TEST_CASE("RME transaction IDs preserve the complete uint32 range", "[rme][regression]") {
    CHECK(rme_protocol::transaction("@RME X tx=0"sv) == 0);
    CHECK(rme_protocol::transaction("@RME X tx=2147483648"sv) == UINT32_C(2147483648));
    CHECK(rme_protocol::transaction("@RME X tx=3933926906"sv) == UINT32_C(3933926906));
    CHECK(rme_protocol::transaction("@RME X tx=4294967295"sv) == std::numeric_limits<uint32_t>::max());
    CHECK_FALSE(rme_protocol::transaction("@RME X tx=4294967296"sv));
    CHECK_FALSE(rme_protocol::transaction("@RME X tx=-1"sv));
    CHECK_FALSE(rme_protocol::transaction("@RME X tx=12x"sv));
    CHECK_FALSE(rme_protocol::transaction("@RME X tx="sv));
}

TEST_CASE("RME numeric parameters reject malformed and overflowing values", "[rme]") {
    CHECK(rme_protocol::signed_number("@RME UI ENCODER value=-100"sv, "value") == -100);
    CHECK(rme_protocol::signed_number("@RME THEME SET primary=#4B2AC3"sv, "primary", 16) == 0x4B2AC3);
    CHECK_FALSE(rme_protocol::signed_number("@RME X value=1garbage"sv, "value"));
    CHECK_FALSE(rme_protocol::signed_number("@RME X value=999999999999999999999"sv, "value"));
    CHECK(rme_protocol::unsigned_number("@RME FILE READ offset=1073741824"sv, "offset") == UINT32_C(1073741824));
    CHECK_FALSE(rme_protocol::unsigned_number("@RME FILE READ offset=-1"sv, "offset"));

    CHECK(rme_protocol::decimal_number("@RME DOCK SET tolerance_x=2.5"sv, "tolerance_x") == 2.5f);
    CHECK(rme_protocol::decimal_number("@RME DOCK SET tolerance_y=1"sv, "tolerance_y") == 1.0f);
    CHECK_FALSE(rme_protocol::decimal_number("@RME DOCK SET tolerance_x=-2.5"sv, "tolerance_x"));
    CHECK_FALSE(rme_protocol::decimal_number("@RME DOCK SET tolerance_x=2.5mm"sv, "tolerance_x"));
    CHECK_FALSE(rme_protocol::decimal_number("@RME DOCK SET tolerance_x=2..5"sv, "tolerance_x"));
    CHECK_FALSE(rme_protocol::decimal_number("@RME DOCK SET tolerance_x=2."sv, "tolerance_x"));
}

TEST_CASE("G427 explicit tool list is authoritative and bounded", "[gcode][g427][indx]") {
    const auto absent = g427_tool_selection::parse("G427 R2 P3", 8);
    CHECK_FALSE(absent.present);
    CHECK_FALSE(absent.mask.has_value());

    const auto selected = g427_tool_selection::parse("G427 T0,7 R2 P3", 8);
    REQUIRE(selected.present);
    REQUIRE(selected.mask.has_value());
    CHECK(*selected.mask == ((uint32_t { 1 } << 0) | (uint32_t { 1 } << 7)));

    CHECK_FALSE(g427_tool_selection::parse("G427 T", 8).mask.has_value());
    CHECK_FALSE(g427_tool_selection::parse("G427 T0,0", 8).mask.has_value());
    CHECK_FALSE(g427_tool_selection::parse("G427 T0,8", 8).mask.has_value());
    CHECK_FALSE(g427_tool_selection::parse("G427 T0,x", 8).mask.has_value());
}

TEST_CASE("INDX dock calibration uses independent configurable axis tolerances", "[rme][indx][dock]") {
    using namespace indx_dock_tolerance;

    CHECK(default_x_mm == 2.5f);
    CHECK(default_y_mm == 1.0f);
    CHECK(accepts_offset(2.5f, 1.0f, default_x_mm, default_y_mm));
    CHECK(accepts_offset(-2.5f, -1.0f, default_x_mm, default_y_mm));
    CHECK_FALSE(accepts_offset(2.56f, 0.0f, default_x_mm, default_y_mm));
    CHECK_FALSE(accepts_offset(0.0f, 1.06f, default_x_mm, default_y_mm));
    CHECK(accepts_offset(3.0f, 0.75f, 3.0f, 0.75f));
    CHECK(sanitize(std::numeric_limits<float>::quiet_NaN(), default_x_mm) == default_x_mm);
}

TEST_CASE("INDX homed back-left corner permits travel over the bed", "[motion][indx][regression]") {
    using namespace buddy::indx_serial_motion_safety;
    constexpr ServiceBoundary boundary { 250, -0.9f };
    // Native INDX X-min / Y-max home, not regular CORE One's opposite corner.
    CHECK(linear_move_is_safe(-1, 206.5f, 9, 206.5f, true, boundary));
    CHECK(linear_move_is_safe(-1, 206.5f, -1, 196.5f, false, boundary));
    CHECK(linear_move_is_safe(4, 201.5f, 125, 100, true, boundary));
    CHECK_FALSE(linear_move_is_safe(245, 100, 255, 100, true, boundary));
    CHECK_FALSE(linear_move_is_safe(125, -10, 135, -10, true, boundary));
}

TEST_CASE("Unknown-axis range helpers clamp to configured boundaries", "[motion][toolchanger][regression]") {
    using namespace buddy::unknown_axis_motion;
    CHECK(assumed_position(AssumedBoundary::minimum, 0, 360) == 0);
    CHECK(assumed_position(AssumedBoundary::maximum, 0, 360) == 360);
    CHECK(constrain(-1, 0, 360) == 0);
    CHECK(constrain(361, 0, 360) == 360);
    CHECK(constrain(125, 0, 360) == 125);
}

TEST_CASE("RME service frames remain isolated from ordinary G-code", "[rme]") {
    CHECK(rme_protocol::is_service_frame("@RME SESSION KEEPALIVE"sv));
    CHECK(rme_protocol::is_service_frame("  @RME FILE ABORT"sv));
    CHECK(rme_protocol::is_service_frame("N123 @RME DIALOG QUERY*7"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("M117 @RME SESSION KEEPALIVE"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("@RME"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("RME SESSION KEEPALIVE"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("=60 visible=1 tx=3933926903"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("N-7"sv));
}

TEST_CASE("RME percent decoding is strict and fixed capacity", "[rme]") {
    const auto decoded = rme_protocol::percent_decode<32>("Prusa%20%2F%20Prusament"sv);
    REQUIRE(decoded);
    CHECK(std::string_view(decoded->data()) == "Prusa / Prusament");

    const auto mixed_case_hex = rme_protocol::percent_decode<16>("3D%20Fuel"sv);
    REQUIRE(mixed_case_hex);
    CHECK(std::string_view(mixed_case_hex->data()) == "3D Fuel");
    CHECK_FALSE(rme_protocol::percent_decode<16>("bad%2"sv));
    CHECK_FALSE(rme_protocol::percent_decode<16>("bad%XZ"sv));
    CHECK_FALSE(rme_protocol::percent_decode<8>("12345678"sv));
    CHECK_FALSE(rme_protocol::percent_decode<16>("zero%00inside"sv));
    CHECK_FALSE(rme_protocol::percent_decode<16>(""sv));
    CHECK(rme_protocol::percent_decode<5>("a"sv).has_value());
}

TEST_CASE("RME filesystem paths are rooted and traversal safe", "[rme][file]") {
    const auto normal = rme_protocol::usb_path<64>("folder%20one/file.gcode"sv);
    REQUIRE(normal);
    CHECK(std::string_view(normal->data()) == "/usb/folder one/file.gcode");

    const auto leading_slashes = rme_protocol::usb_path<64>("///FWUPD.BBF"sv);
    REQUIRE(leading_slashes);
    CHECK(std::string_view(leading_slashes->data()) == "/usb/FWUPD.BBF");

    const auto root = rme_protocol::usb_path<64>(""sv);
    REQUIRE(root);
    CHECK(std::string_view(root->data()) == "/usb/");

    CHECK_FALSE(rme_protocol::usb_path<64>("../secret"sv));
    CHECK_FALSE(rme_protocol::usb_path<64>("safe/%2e%2e/secret"sv));
    CHECK_FALSE(rme_protocol::usb_path<64>("safe//file"sv));
    CHECK_FALSE(rme_protocol::usb_path<16>("this-name-is-too-long.gcode"sv));
    CHECK_FALSE(rme_protocol::usb_path<5>("file"sv));
    CHECK_FALSE(rme_protocol::usb_path<8>("abc"sv));
}

TEST_CASE("RME SHA-256 parsing is exact", "[rme][file]") {
    std::array<uint8_t, 32> digest {};
    REQUIRE(rme_protocol::parse_sha256("000102030405060708090a0b0c0d0e0f101112131415161718191A1B1C1D1E1F"sv, digest));
    for (size_t i = 0; i < digest.size(); ++i) {
        CHECK(digest[i] == i);
    }
    CHECK_FALSE(rme_protocol::parse_sha256("00"sv, digest));
    CHECK_FALSE(rme_protocol::parse_sha256("000102030405060708090a0b0c0d0e0f101112131415161718191A1B1C1D1E1Z"sv, digest));
}

TEST_CASE("RME parser state does not accumulate across repeated synchronization", "[rme][stress]") {
    constexpr auto command = "@RME FILAMENT SET slot=4 name=PLA-00H nozzle=215 preheat=175 bed=60 visible=1 tx=3933926903"sv;
    for (size_t iteration = 0; iteration < 100000; ++iteration) {
        REQUIRE(rme_protocol::is_service_frame(command));
        REQUIRE(rme_protocol::unsigned_number(command, "slot") == 4);
        REQUIRE(rme_protocol::value(command, "name") == "PLA-00H");
        REQUIRE(rme_protocol::transaction(command) == UINT32_C(3933926903));
    }
}

TEST_CASE("RME serial dispatch rejects recursive draining", "[rme][file][stream][regression]") {
    bool active = false;
    {
        rme_protocol::ScopedDispatchGuard outer { active };
        REQUIRE(outer);
        CHECK(active);

        // A filesystem wait may service the main loop while the outer bulk
        // frame still owns the receive buffer.  The nested reader must leave
        // all byte-stream state untouched.
        rme_protocol::ScopedDispatchGuard nested { active };
        CHECK_FALSE(nested);
        CHECK(active);
    }
    CHECK_FALSE(active);

    // The next scheduler pass can drain the already-buffered CDC bytes.
    rme_protocol::ScopedDispatchGuard next_pass { active };
    CHECK(next_pass);
}

TEST_CASE("RME binary transfer failures have stable diagnostics", "[rme][file]") {
    using rme_file_transfer::BinaryFrameError;
    using rme_file_transfer::classify_binary_frame;
    using rme_file_transfer::diagnostic_code;

    CHECK(classify_binary_frame(12288, 12288, 1024, 4096000, true) == BinaryFrameError::none);
    CHECK(classify_binary_frame(13312, 12288, 1024, 4096000, true) == BinaryFrameError::offset_mismatch);
    CHECK(classify_binary_frame(4096000, 4096000, 1, 4096000, true) == BinaryFrameError::size_exceeded);
    CHECK(classify_binary_frame(12288, 12288, 1024, 4096000, false) == BinaryFrameError::crc_mismatch);
    CHECK(classify_binary_frame(0, 1, 1, 0, true) == BinaryFrameError::offset_mismatch);

    CHECK(std::string_view(diagnostic_code(BinaryFrameError::offset_mismatch)) == "offset_mismatch");
    CHECK(std::string_view(diagnostic_code(BinaryFrameError::size_exceeded)) == "size_exceeded");
    CHECK(std::string_view(diagnostic_code(BinaryFrameError::crc_mismatch)) == "crc_mismatch");
    CHECK(diagnostic_code(BinaryFrameError::none) == nullptr);
    CHECK(std::string_view(diagnostic_code(static_cast<BinaryFrameError>(255))) == "invalid_frame");
}

TEST_CASE("RME text bulk receive backlog matches the advertised window", "[rme][file][regression]") {
    CHECK(rme_file_transfer::bulk_payload_size == 384);
    CHECK(rme_file_transfer::bulk_window_size == 4);
    CHECK(rme_file_transfer::bulk_base64_size == 512);
    CHECK(rme_file_transfer::bulk_receive_backlog <= 2048);
    CHECK(rme_file_transfer::bulk_receive_backlog > 512);
}

TEST_CASE("RME binary window fits completely in the target CDC FIFO", "[rme][file][stream][regression]") {
    CHECK(rme_file_transfer::binary_header_size == 10);
    CHECK(rme_file_transfer::binary_payload_size == 512);
    CHECK(rme_file_transfer::binary_window_size == 3);
    CHECK(rme_file_transfer::binary_receive_backlog == 1566);
    CHECK(rme_file_transfer::binary_receive_backlog <= 2048);
}

TEST_CASE("RME text abort escapes malformed binary mode", "[rme][file][regression]") {
    uint8_t matched = 0;
    constexpr std::string_view abort = "@RME FILE ABORT\n";
    for (size_t i = 0; i + 1 < abort.size(); ++i) {
        CHECK_FALSE(rme_file_transfer::consume_text_abort(abort[i], matched, i >= 9));
    }
    CHECK(rme_file_transfer::consume_text_abort(abort.back(), matched, true));
    CHECK(matched == 0);

    // Valid binary payload bytes must never activate the compatibility escape.
    for (const char byte : abort) {
        CHECK_FALSE(rme_file_transfer::consume_text_abort(byte, matched, false));
    }
}

TEST_CASE("RME binary control offsets do not overlap data", "[rme][file]") {
    CHECK(rme_file_transfer::control_frame_offset == UINT32_C(0xfffffffe));
    CHECK(rme_file_transfer::abort_frame_offset == UINT32_C(0xffffffff));
    CHECK(rme_file_transfer::control_frame_offset != rme_file_transfer::abort_frame_offset);
    CHECK(rme_file_transfer::control_frame_offset > UINT32_C(0x3fffffff));
}

TEST_CASE("RME binary header recovery rejects untrusted lengths without blind discard", "[rme][file][regression]") {
    using rme_file_transfer::plausible_binary_header;
    CHECK(plausible_binary_header(12288, 1024, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(12288, 65535, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(13312, 1024, 12288, 4096000, 1024));
    CHECK(plausible_binary_header(rme_file_transfer::abort_frame_offset, 0, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(rme_file_transfer::abort_frame_offset, 1, 12288, 4096000, 1024));
    CHECK(plausible_binary_header(rme_file_transfer::control_frame_offset, 32, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(rme_file_transfer::control_frame_offset, 0, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(rme_file_transfer::control_frame_offset, 1025, 12288, 4096000, 1024));
}

TEST_CASE("RME rolling binary header recovery finds abort after a corrupt length", "[rme][file][stream]") {
    auto append_header = [](std::vector<uint8_t> &stream, const uint32_t offset, const uint16_t length, const uint32_t crc) {
        stream.push_back(offset);
        stream.push_back(offset >> 8);
        stream.push_back(offset >> 16);
        stream.push_back(offset >> 24);
        stream.push_back(length);
        stream.push_back(length >> 8);
        stream.push_back(crc);
        stream.push_back(crc >> 8);
        stream.push_back(crc >> 16);
        stream.push_back(crc >> 24);
    };
    std::vector<uint8_t> stream;
    append_header(stream, 12288, 65535, 0x12345678); // untrusted/corrupt
    stream.insert(stream.end(), { 0xaa, 0xbb, 0xcc, 0xdd, 0xee });
    append_header(stream, rme_file_transfer::abort_frame_offset, 0, 0);

    std::array<uint8_t, rme_file_transfer::binary_header_size> window {};
    uint16_t filled = 0;
    uint32_t offset = 0;
    uint16_t length = 0;
    uint32_t crc = 0;
    bool found_abort = false;
    for (const uint8_t byte : stream) {
        const auto result = rme_file_transfer::scan_binary_header_byte(
            window, filled, byte, 12288, 4096000, 1024, offset, length, crc);
        if (result == rme_file_transfer::HeaderScanResult::ready
            && offset == rme_file_transfer::abort_frame_offset && length == 0) {
            found_abort = true;
            break;
        }
    }
    CHECK(found_abort);
}

TEST_CASE("RME production header scanner covers data and control frame states", "[rme][file][stream]") {
    auto header = [](const uint32_t offset, const uint16_t length, const uint32_t crc) {
        return std::array<uint8_t, rme_file_transfer::binary_header_size> {
            uint8_t(offset),
            uint8_t(offset >> 8),
            uint8_t(offset >> 16),
            uint8_t(offset >> 24),
            uint8_t(length),
            uint8_t(length >> 8),
            uint8_t(crc),
            uint8_t(crc >> 8),
            uint8_t(crc >> 16),
            uint8_t(crc >> 24),
        };
    };
    auto scan = [](const auto &bytes, const uint32_t committed, const uint32_t expected,
                    uint32_t &offset, uint16_t &length, uint32_t &crc) {
        std::array<uint8_t, rme_file_transfer::binary_header_size> window {};
        uint16_t received = 0;
        auto result = rme_file_transfer::HeaderScanResult::incomplete;
        for (const uint8_t byte : bytes) {
            result = rme_file_transfer::scan_binary_header_byte(
                window, received, byte, committed, expected,
                rme_file_transfer::binary_payload_size, offset, length, crc);
        }
        return result;
    };

    uint32_t offset = 0;
    uint16_t length = 0;
    uint32_t crc = 0;
    const auto data = header(1536, 512, UINT32_C(0x78563412));
    CHECK(scan(data, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::ready);
    CHECK(offset == 1536);
    CHECK(length == 512);
    CHECK(crc == UINT32_C(0x78563412));

    std::array<uint8_t, rme_file_transfer::binary_header_size> window {};
    uint16_t received = 0;
    for (size_t index = 0; index + 1 < data.size(); ++index) {
        CHECK(rme_file_transfer::scan_binary_header_byte(
                  window, received, data[index], 1536, 4096, 512,
                  offset, length, crc)
            == rme_file_transfer::HeaderScanResult::incomplete);
    }

    const auto wrong_offset = header(2048, 512, 0);
    CHECK(scan(wrong_offset, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::invalid);
    const auto control = header(rme_file_transfer::control_frame_offset, 16, 1);
    CHECK(scan(control, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::ready);
    const auto abort = header(rme_file_transfer::abort_frame_offset, 0, 0);
    CHECK(scan(abort, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::ready);
}
TEST_CASE("firmware candidate cleanup waits for bootloader handoff") {
    CHECK_FALSE(firmware_update_handoff::candidate_cleanup_allowed(true));
    CHECK(firmware_update_handoff::candidate_cleanup_allowed(false));
}

TEST_CASE("firmware cleanup marker waits for storage ownership and is checked once", "[rme][file][firmware]") {
    firmware_update_handoff::CleanupGate gate;

    CHECK_FALSE(gate.should_check(false, false));
    CHECK_FALSE(gate.should_check(true, true));
    CHECK(gate.should_check(true, false));
    CHECK_FALSE(gate.should_check(true, false));
    CHECK_FALSE(gate.should_check(true, true));

    // Removing and reinserting the medium starts a new inspection lifetime.
    CHECK_FALSE(gate.should_check(false, false));
    CHECK(gate.should_check(true, false));
}

TEST_CASE("Auto PA uses gentle flexible filament feeds", "[rme][pa]") {
    using namespace buddy::m976_extrusion_policy;
    CHECK(is_flexible("FLEX-001", "FLEX", false));
    CHECK(is_flexible("custom spool", "TPU", false));
    CHECK(is_flexible("TPE-001", "", false));
    CHECK(is_flexible("custom spool", "", true));
    CHECK_FALSE(is_flexible("PLA-001", "PLA", false));
    CHECK_FALSE(is_flexible("FLEX-named spool", "PETG", false));
    const auto flex = speeds(true);
    const auto rigid = speeds(false);
    CHECK(flex.low_mm_s == 0.2f);
    CHECK(flex.high_mm_s == 1.0f);
    CHECK(flex.retract_mm_s == 2.0f);
    CHECK(flex.high_mm_s - flex.low_mm_s > 0.5f);
    CHECK(flex.high_mm_s * 2.4053f < 2.5f);
    CHECK(rigid.low_mm_s == 0.8f);
    CHECK(rigid.high_mm_s == 8.0f);
    CHECK(rigid.retract_mm_s == 20.0f);
}

TEST_CASE("RME light hold is transient and print-safe", "[rme][light]") {
    rme_light_hold::State hold;
    CHECK_FALSE(hold.active());
    CHECK(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    CHECK(hold.active());
    CHECK(hold.set_from_host(true, false) == rme_light_hold::SetResult::unchanged);
    CHECK_FALSE(hold.consume_automatic_release());

    hold.release_automatically();
    CHECK_FALSE(hold.active());
    CHECK(hold.consume_automatic_release());
    CHECK_FALSE(hold.consume_automatic_release());

    CHECK(hold.set_from_host(true, true) == rme_light_hold::SetResult::printer_busy);
    CHECK_FALSE(hold.active());
}

TEST_CASE("RME light hold session release cannot resume", "[rme][light]") {
    rme_light_hold::State hold;
    REQUIRE(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    hold.release_with_session();
    CHECK_FALSE(hold.active());
    CHECK_FALSE(hold.consume_automatic_release());

    REQUIRE(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    hold.release_automatically();
    CHECK_FALSE(hold.active());
    CHECK(hold.consume_automatic_release());
    CHECK_FALSE(hold.active());
}

TEST_CASE("RME light hold automatic release is edge triggered", "[rme][light]") {
    rme_light_hold::State hold;
    hold.release_automatically();
    CHECK_FALSE(hold.consume_automatic_release());

    REQUIRE(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    hold.release_automatically();
    hold.release_automatically();
    CHECK(hold.consume_automatic_release());
    CHECK_FALSE(hold.consume_automatic_release());

    REQUIRE(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    hold.release_with_session();
    CHECK_FALSE(hold.consume_automatic_release());
}

TEST_CASE("RME light hold queries cannot mutate or revive state", "[rme][light]") {
    rme_light_hold::State hold;
    REQUIRE(hold.set_from_host(true, false) == rme_light_hold::SetResult::changed);
    CHECK(hold.active());
    CHECK(hold.active());
    CHECK_FALSE(hold.consume_automatic_release());

    hold.release_automatically();
    CHECK_FALSE(hold.active());
    CHECK_FALSE(hold.active());
    CHECK(hold.consume_automatic_release());
    CHECK_FALSE(hold.active());
}

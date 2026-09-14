#pragma once

#include <cstdint>

namespace rme_indx_workflow {

struct Descriptor {
    const char *workflow;
    const char *phase;
    bool error;
};

// Kept independent of the GUI/FSM implementation so every INDX phase can be
// exhaustively unit-tested without pulling the firmware configuration into the
// host test binary.
constexpr Descriptor nozzle_mismatch(const uint8_t phase) {
    switch (phase) {
    case 0:
        return { "indx_tool_detection", "unknown_tool_detected", false };
    case 1:
        return { "indx_slot_selection", "select_slot", false };
    case 2:
        return { "indx_slot_selection", "parking", false };
    case 3:
        return { "indx_slot_selection", "dock_not_empty", true };
    case 4:
        return { "indx_tool_detection", "tool_lost", true };
    case 5:
        return { "indx_tool_detection", "homing", false };
    case 6:
        return { "indx_tool_change", "pickup_failed", true };
    case 7:
        return { "indx_tool_change", "park_failed", true };
    case 8:
        return { "indx_tool_change", "confirm_abort", true };
    default:
        return { "indx", "unknown", true };
    }
}

constexpr const char *dock_calibration_phase(const uint8_t phase) {
    constexpr const char *names[] = {
        "intro",
        "remove_tool",
        "select_dock_count",
        "select_docks",
        "homing",
        "moving_away",
        "parking_tool",
        "tighten_screws",
        "position_dock",
        "lock_position",
        "measuring",
        "validating",
        "loosen_bolts",
        "success",
        "failed",
    };
    return phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "unknown";
}

constexpr const char *nozzle_cleaner_phase(const uint8_t phase) {
    constexpr const char *names[] = {
        "intro",
        "cooldown",
        "picking_tool",
        "homing",
        "moving_away",
        "position_z",
        "position_x",
        "lock_x",
        "measuring_x",
        "evaluating_x",
        "clean_nozzle",
        "position_y",
        "lock_y",
        "measuring_y",
        "evaluating_y",
        "success",
    };
    return phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "unknown";
}

constexpr const char *tool_offsets_phase(const uint8_t phase) {
    constexpr const char *names[] = {
        "intro",
        "clean_nozzles",
        "moving_away",
        "picking_tool",
        "homing",
        "calibrating",
        "success",
        "failed",
    };
    return phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "unknown";
}

} // namespace rme_indx_workflow

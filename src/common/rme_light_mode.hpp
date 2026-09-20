#pragma once
#include <cstdint>
#include <rme_light_hold.hpp>

namespace rme_light_mode {
// External bars are switched outputs, so an illuminated bar contributes 100%.
constexpr uint8_t reported_brightness(uint8_t internal, bool external_on) {
    return external_on ? 255 : internal;
}

constexpr uint8_t reported_mode(bool printing, bool locked, uint8_t brightness) {
    return !printing && locked ? 2 : (brightness > 0 ? 1 : 0);
}

// Print overrides are independent of idle activity, door holds and timers.
struct PrintState {
    int8_t mode = -1;
    void set(uint8_t value) { mode = value ? 1 : 0; }
    void reset() { mode = -1; }
    uint8_t brightness(bool print_enabled, uint8_t print_brightness, bool active_enabled, uint8_t active_brightness) const {
        // Explicit On selects the Active profile even during a print. Only
        // automatic mode uses the print profile; Off blanks every channel.
        if (mode == 0) {
            return 0;
        }
        return mode > 0 ? (active_enabled ? active_brightness : 0) : (print_enabled ? print_brightness : 0);
    }
};
// Streamed job commands are machine activity, not a user waking the lights.
// Idle jog/heating commands still resume automatic lighting normally.
constexpr bool serial_commands_wake_lights(bool serial_print_active) {
    return !serial_print_active;
}

constexpr uint8_t active_profile_brightness(int8_t mode, bool enabled, uint8_t brightness) {
    return mode > 0 && enabled ? brightness : 0;
}
// Shared LCD/status idle timer must resume even after a legacy locked hold.
inline void restart_idle_countdown(rme_light_hold::State &hold, uint32_t &timestamp, uint32_t now, bool door_holds_active = false) {
    hold.release_automatically();
    if (!door_holds_active) {
        timestamp = now ? now : UINT32_MAX; // Zero is inactive; bias one tick into the past.
    }
}

// Caller owns synchronization. Fixed storage; no timers, queues or allocation.
struct State {
    // An open-door hold is a level, not only an opening event. Temporary
    // Off/On must not defeat it; explicit Locked retains its semantics.
    bool apply_door_hold(bool held) {
        if (!held || (mode != 0 && mode != 1)) {
            return false;
        }
        mode = -1;
        return true;
    }
    // Off is temporary darkness, not a lock against normal printer activity.
    bool resume_on_activity() {
        if (mode != 0) {
            return false;
        }
        mode = -1;
        return true;
    }
    int8_t mode = -1;
    uint32_t started_ms = 0;
    bool set(uint8_t value, uint32_t now) {
        if (value > 2) {
            return false;
        }
        mode = value;
        started_ms = now;
        return true;
    }
    bool expire(uint32_t now, uint16_t timeout_s) {
        if (mode != 1 || uint32_t(now - started_ms) < uint32_t(timeout_s) * 1000) {
            return false;
        }
        mode = 0;
        return true;
    }
};
} // namespace rme_light_mode

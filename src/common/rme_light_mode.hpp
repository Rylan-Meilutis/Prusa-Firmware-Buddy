#pragma once
#include <cstdint>
#include <rme_light_hold.hpp>

namespace rme_light_mode {
// Shared LCD/status idle timer must resume even after a legacy locked hold.
inline void restart_idle_countdown(rme_light_hold::State &hold, uint32_t &timestamp, uint32_t now) {
    hold.release_automatically();
    timestamp = now ? now : UINT32_MAX; // Zero is inactive; bias one tick into the past.
}

// Caller owns synchronization. Fixed storage; no timers, queues or allocation.
struct State {
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

#pragma once
#include <cstdint>

namespace rme_light_mode {
// Caller owns synchronization. Fixed storage; no timers, queues or allocation.
struct State {
    int8_t mode = -1;
    uint32_t started_ms = 0;
    bool set(uint8_t value, uint32_t now) {
        if (value > 2) return false;
        mode = value;
        started_ms = now;
        return true;
    }
    bool expire(uint32_t now, uint16_t timeout_s) {
        if (mode != 1 || uint32_t(now - started_ms) < uint32_t(timeout_s) * 1000) return false;
        mode = 0;
        return true;
    }
};
}

#pragma once
#include <cstdint>

namespace rme_tool_change_progress {
// Host-indexed queued-file transitions, not measured physical tool changes.
struct State {
    uint32_t remaining = 0;
    uint32_t total = 0;
    uint32_t received_ms = 0;
    bool received = false;
    bool set(uint32_t left, uint32_t count, uint32_t now) {
        if (left > count || count > 200000) {
            return false;
        }
        remaining = left;
        total = count;
        received_ms = now;
        received = true;
        return true;
    }
    bool valid(uint32_t now) const { return received && uint32_t(now - received_ms) <= 60000; }
};
} // namespace rme_tool_change_progress

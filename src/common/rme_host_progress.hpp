#pragma once
#include <cstdint>
#include <limits>

namespace rme_host_progress {
struct State {
    static constexpr uint32_t unknown = std::numeric_limits<uint32_t>::max();
    uint32_t received_ms = 0;
    uint32_t remaining = unknown;
    uint8_t percent = 0;
    bool paused = false;
    bool received = false;
    bool valid(uint32_t now) const { return received && uint32_t(now - received_ms) <= 60000; }
    void set(uint8_t progress, uint32_t seconds, bool pause, uint32_t now) {
        percent = progress;
        remaining = seconds;
        paused = pause;
        received_ms = now;
        received = true;
    }
    uint32_t seconds(uint32_t now) const {
        if (remaining == unknown || paused) {
            return remaining;
        }
        const auto elapsed = uint32_t(now - received_ms) / 1000;
        return elapsed < remaining ? remaining - elapsed : 0;
    }
};
} // namespace rme_host_progress

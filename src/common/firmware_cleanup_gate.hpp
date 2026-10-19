#pragma once

#include <cstdint>

namespace firmware_update_handoff {

/// Inspect once after a conclusive result, but retry transient filesystem
/// failures at most once per second. Never poll storage on every server loop.
class CleanupGate {
public:
    bool should_check(bool media_inserted, bool transfer_active, uint32_t now_ms) {
        if (!media_inserted) {
            checked_ = false;
            attempted_ = false;
            return false;
        }
        if (checked_ || transfer_active || (attempted_ && uint32_t(now_ms - last_attempt_ms_) < 1000)) {
            return false;
        }
        attempted_ = true;
        last_attempt_ms_ = now_ms;
        return true;
    }

    void complete() { checked_ = true; }

private:
    bool checked_ = false;
    bool attempted_ = false;
    uint32_t last_attempt_ms_ = 0;
};

} // namespace firmware_update_handoff

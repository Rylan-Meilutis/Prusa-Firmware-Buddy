#pragma once

#include <cstdint>

namespace firmware_update_handoff {

/// Stop after successful cleanup; retry I/O failures once per second and an
/// absent marker every 30 seconds. ENOENT must not latch off cleanup forever.
class CleanupGate {
public:
    bool should_check(bool media_inserted, bool transfer_active, uint32_t now_ms) {
        if (!media_inserted) {
            checked_ = false;
            attempted_ = false;
            marker_missing_ = false;
            return false;
        }
        const uint32_t retry_ms = marker_missing_ ? 30000 : 1000;
        if (checked_ || transfer_active || (attempted_ && uint32_t(now_ms - last_attempt_ms_) < retry_ms)) {
            return false;
        }
        attempted_ = true;
        last_attempt_ms_ = now_ms;
        marker_missing_ = false;
        return true;
    }

    void complete() { checked_ = true; }
    void marker_absent() { marker_missing_ = true; }

private:
    bool checked_ = false;
    bool attempted_ = false;
    bool marker_missing_ = false;
    uint32_t last_attempt_ms_ = 0;
};

} // namespace firmware_update_handoff

#pragma once

#include <cstdint>

namespace buddy::puppies {

/// Deadlines are based on actual flashing progress, not repeated status polls.
/// A required sensor must either become ready or enter recovery, never be skipped.
class ToolSensorBootstrapDeadline {
public:
    explicit ToolSensorBootstrapDeadline(uint32_t now)
        : started_(now)
        , last_progress_(now) {}

    bool stalled(uint32_t now, bool flashing, uint8_t progress,
        uint32_t verify_timeout = 15'000, uint32_t flash_timeout = 60'000,
        uint32_t attempt_timeout = 300'000) {
        if (flashing != flashing_ || (flashing && progress > progress_)) {
            last_progress_ = now;
        }
        flashing_ = flashing;
        progress_ = progress;
        return uint32_t(now - started_) >= attempt_timeout
            || uint32_t(now - last_progress_) >= (flashing ? flash_timeout : verify_timeout);
    }

private:
    uint32_t started_;
    uint32_t last_progress_;
    uint8_t progress_ = 0;
    bool flashing_ = false;
};

} // namespace buddy::puppies

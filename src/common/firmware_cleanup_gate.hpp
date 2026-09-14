#pragma once

namespace firmware_update_handoff {

/// Permits one firmware-marker inspection per mounted-media lifetime, while
/// deferring it behind the shared storage-transfer owner.
class CleanupGate {
public:
    bool should_check(bool media_inserted, bool transfer_active) {
        if (!media_inserted) {
            checked_ = false;
            return false;
        }
        if (checked_ || transfer_active) {
            return false;
        }
        checked_ = true;
        return true;
    }

private:
    bool checked_ = false;
};

} // namespace firmware_update_handoff

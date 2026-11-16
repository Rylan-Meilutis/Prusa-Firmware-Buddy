#pragma once

#include <cstdint>

namespace buddy::indx_toolchange_ejection_policy {

/// Tracks only an explicit, successful print-time G12 S90 ... S91 visit.
/// Maintenance/direct cleaner calls do not pass through this policy.
class Visit {
public:
    void reset() {
        active_ = false;
        completed_ejections_ = 0;
    }

    void begin(bool eligible, uint8_t tool) {
        reset();
        active_ = eligible;
        tool_ = tool;
    }

    bool skip_exit(bool entry_only, bool eligible, uint8_t tool, bool force) {
        if (!eligible || tool != tool_) {
            reset();
        }
        // Never omit the first ejection, a failed first ejection, or an
        // unexpected third/recovery request. Consume at most one exit request.
        if (entry_only && !force && active_ && completed_ejections_ == 1) {
            completed_ejections_ = 2;
            return true;
        }
        return false;
    }

    void ejection_finished(bool success) {
        if (!success) {
            reset();
        } else if (active_ && completed_ejections_ < 2) {
            ++completed_ejections_;
        }
    }

private:
    bool active_ = false;
    uint8_t tool_ = 0;
    uint8_t completed_ejections_ = 0;
};

} // namespace buddy::indx_toolchange_ejection_policy

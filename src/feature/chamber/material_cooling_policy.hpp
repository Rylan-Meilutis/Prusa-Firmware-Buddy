#pragma once

#include <string_view>

namespace buddy::chamber_material {

// Server-thread-only, allocation-free job state. Metadata can only restrict a
// job once forbidden material has been reported/observed, never re-enable it.
struct SerialJobPolicy {
    bool complete_and_permitted = false;
    bool forbidden_seen = false;
    void reset() { *this = {}; }
    void report(bool permitted) {
        complete_and_permitted = permitted;
        forbidden_seen |= !permitted;
    }
    void observe(bool permitted) { forbidden_seen |= !permitted; }
    bool permits() const { return complete_and_permitted && !forbidden_seen; }
};
inline SerialJobPolicy serial_job;

// Deliberately allow-list low-temperature polymers. Unknown families and custom
// profiles without a known base must not silently cool a warp-sensitive print.
constexpr bool permits_cooling(std::string_view family) {
    return family == "PLA" || family == "PETG" || family == "PET" || family == "FLEX" || family == "TPU" || family == "TPE";
}

// Apply before emergency thermal overrides, never after them. DIY filters use
// the chamber fans themselves: their filtration minimum must be preserved.
constexpr unsigned normal_pwm(unsigned requested, bool enabled, bool printing, bool permitted, unsigned filtration_minimum = 0, bool fans_enabled = true) {
    return enabled && printing && (!permitted || !fans_enabled) ? filtration_minimum : requested;
}

} // namespace buddy::chamber_material

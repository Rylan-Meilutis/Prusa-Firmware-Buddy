#pragma once

namespace firmware_update_handoff {

/// A cleanup marker means "remove after the bootloader attempt", but it can
/// become visible to the application server before M997 resets.  The retained
/// exact-file request is therefore authoritative while it remains armed.
constexpr bool candidate_cleanup_allowed(const bool exact_candidate_armed) {
    return !exact_candidate_armed;
}

} // namespace firmware_update_handoff

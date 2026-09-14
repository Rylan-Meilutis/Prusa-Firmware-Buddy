#pragma once

#include <cstdint>

namespace buddy::m976_indx_policy {

// INDX docks occupy the far-right service area used by the legacy loadcell
// sheet-cleaning probe.  No M976 phase may enter that rectangle: INDX has a
// calibrated, keep-out-aware nozzle cleaner for this purpose.
constexpr bool uses_sheet_contact_cleanup(const bool has_indx, const bool cleanup_supported) {
    return cleanup_supported && !has_indx;
}

// Validate every externally supplied manifest association before the first
// tool-change. This is intentionally independent of motion code so malformed
// and stale mappings can be exhaustively tested on the host.
constexpr bool safe_manifest_mapping(const uint8_t requested_physical,
    const uint8_t requested_logical, const uint8_t mapped_physical,
    const bool physical_enabled, const bool logical_enabled) {
    (void)requested_logical;
    return physical_enabled && logical_enabled
        && requested_physical == mapped_physical;
}

} // namespace buddy::m976_indx_policy

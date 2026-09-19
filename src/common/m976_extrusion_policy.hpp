#pragma once

#include <m976_material.hpp>

namespace buddy::m976_extrusion_policy {

struct Speeds {
    float low_mm_s;
    float high_mm_s;
    float retract_mm_s;
};

constexpr bool is_flexible(std::string_view profile, std::string_view base, bool flexible_flag) {
    const auto material = m976_material::authoritative_name(profile, base);
    return flexible_flag || material.starts_with("FLEX")
        || material.starts_with("TPU") || material.starts_with("TPE");
}

constexpr Speeds speeds(bool flexible) {
    // Filament feed speeds, not nozzle travel or volumetric flow. Keep a
    // >1 mm/s transition for the loadcell scorer, below 4 mm3/s for 1.75 mm
    // flexible filament. Confidence gates must still accept the measurement.
    return flexible ? Speeds { 0.2f, 1.5f, 2.0f } : Speeds { 0.8f, 8.0f, 20.0f };
}

} // namespace buddy::m976_extrusion_policy

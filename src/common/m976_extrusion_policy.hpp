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
    return flexible_flag || filament_material::is_flexible_family(material);
}

constexpr Speeds speeds(bool flexible) {
    // Filament feed speeds, not nozzle travel or volumetric flow. Keep a
    // measurable transition with windowed E-step scoring, below 2.5 mm3/s
    // for 1.75 mm flexible filament. Confidence gates remain mandatory.
    return flexible ? Speeds { 0.2f, 1.0f, 2.0f } : Speeds { 0.8f, 8.0f, 20.0f };
}

} // namespace buddy::m976_extrusion_policy

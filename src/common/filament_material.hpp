#pragma once

#include <string_view>

namespace buddy::filament_material {

/// Flexible-family aliases, including legacy profile suffixes and grades.
constexpr bool is_flexible_family(const std::string_view name) {
    return name.starts_with("FLEX") || name.starts_with("TPU") || name.starts_with("TPE");
}

/// Only explicit family aliases are interchangeable. Never strip profile
/// suffixes or specialty grades: PLA-CF is not plain PLA, nor TPU-95A FLEX.
constexpr std::string_view canonical_name(const std::string_view name) {
    if (name == "TPU" || name == "TPE") {
        return "FLEX";
    }
    if (name == "PET") {
        return "PETG";
    }
    if (name == "NYLON") {
        return "PA";
    }
    return name;
}

constexpr bool matches(const std::string_view requested, const std::string_view loaded) {
    return !requested.empty() && requested != "---" && canonical_name(requested) == canonical_name(loaded);
}

/// A profile identifies saved settings; material identifies the polymer.
/// Prefer the configured base material and use the profile only for legacy
/// profiles which do not provide a base.
constexpr std::string_view authoritative_name(const std::string_view profile_name, const std::string_view base_name) {
    return base_name.empty() ? profile_name : base_name;
}

} // namespace buddy::filament_material

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

/// Old generated profile IDs without material metadata are unknown, not a
/// polymer inferred from their prefix. The host can repair their stored base
/// using an exact provider identity match before starting a print.
constexpr std::string_view legacy_profile_material(const std::string_view profile) {
    if (profile.size() == 7 && profile.front() == 'S') {
        bool opaque = true;
        for (size_t i = 1; i < profile.size(); ++i) {
            opaque &= (profile[i] >= '0' && profile[i] <= '9') || (profile[i] >= 'A' && profile[i] <= 'Z');
        }
        if (opaque) {
            return {};
        }
    }
    if (profile.size() == 7 && profile[3] == '-' && profile[4] == '0') {
        for (size_t i = 5; i < 7; ++i) {
            if (!((profile[i] >= '0' && profile[i] <= '9') || (profile[i] >= 'A' && profile[i] <= 'Z'))) {
                return profile;
            }
        }
        return {};
    }
    return profile;
}

/// A profile identifies saved settings; material identifies the polymer.
/// An explicitly configured base always overrides a legacy profile name.
constexpr std::string_view authoritative_name(const std::string_view profile_name, const std::string_view base_name) {
    return base_name.empty() ? legacy_profile_material(profile_name) : base_name;
}

} // namespace buddy::filament_material

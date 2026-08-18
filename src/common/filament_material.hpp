#pragma once

#include <string_view>

namespace buddy::filament_material {

/// A profile identifies saved settings; material identifies the polymer.
/// Prefer the configured base material and use the profile only for legacy
/// profiles which do not provide a base.
constexpr std::string_view authoritative_name(const std::string_view profile_name, const std::string_view base_name) {
    return base_name.empty() ? profile_name : base_name;
}

} // namespace buddy::filament_material

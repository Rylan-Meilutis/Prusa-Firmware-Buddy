#pragma once

#include <string_view>

namespace buddy::m976_material {

constexpr std::string_view authoritative_name(const std::string_view profile_name, const std::string_view base_name) {
    return base_name.empty() ? profile_name : base_name;
}

constexpr bool matches(const std::string_view requested, const std::string_view profile_name, const std::string_view base_name) {
    return requested == authoritative_name(profile_name, base_name)
        || (!base_name.empty() && requested == profile_name);
}

constexpr float fallback(const std::string_view profile_name, const std::string_view base_name) {
    const auto material = authoritative_name(profile_name, base_name);
    if (material.starts_with("FLEX")) return 0.08f;
    if (material.starts_with("PETG")) return 0.045f;
    if (material.starts_with("PA")) return 0.05f;
    return 0.04f;
}

} // namespace buddy::m976_material

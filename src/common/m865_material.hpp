#pragma once

#include <array>
#include <filament.hpp>
#include <gcode/gcode_parser.hpp>

namespace buddy::m865_material {

#if HAS_FILAMENT_MATERIAL_FAMILY_PARAM()
/// Parse into capacity, not the current length of an empty InplaceString.
/// Return false on malformed/invalid J so M865 cannot commit a partial profile.
inline bool apply_base_option(const GCodeParser2 &parser, FilamentTypeParameters &params) {
    if (!parser.has_option('J')) {
        return true;
    }
    std::array<char, filament_name_buffer_size> buffer {};
    const auto name = parser.option<std::string_view>('J', buffer);
    if (!name) {
        return false;
    }
    if (name->empty()) {
        params.base_preset = std::nullopt;
        return true;
    }
    const auto type = FilamentType::from_name(*name);
    if (const auto preset = std::get_if<PresetFilamentType>(&type)) {
        params.base_preset = *preset;
        return true;
    }
    parser.report_option_error('J', "Needs to be a preset filament type");
    return false;
}
#endif

} // namespace buddy::m865_material

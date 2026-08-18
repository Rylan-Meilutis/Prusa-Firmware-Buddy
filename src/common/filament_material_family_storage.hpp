#pragma once

#include <cstdint>
#include <optional>

namespace buddy::filament_material_family_storage {

// Zero is the value written by all firmware predating material-family
// persistence. Presets are one-based so old records remain "unset".
constexpr uint8_t encode(const std::optional<uint8_t> preset) {
    return preset.has_value() ? static_cast<uint8_t>(*preset + 1) : 0;
}

constexpr std::optional<uint8_t> decode(const uint8_t stored, const uint8_t preset_count) {
    if (stored == 0 || stored > preset_count) {
        return std::nullopt;
    }
    return static_cast<uint8_t>(stored - 1);
}

} // namespace buddy::filament_material_family_storage

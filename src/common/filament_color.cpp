#include "filament_color.hpp"

#include <config_store/store_instance.hpp>

#include <algorithm>
#include <cstring>

namespace filament_color {
namespace {
#if HAS_MINI_DISPLAY()
constexpr std::array<PaletteProfile, 2> profiles {{
    { "Black", Color::from_raw(0x000000) }, { "White", Color::from_raw(0xffffff) },
}};
#else
constexpr std::array<PaletteProfile, 15> profiles {{
    { "Black", Color::from_raw(0x000000) }, { "Blue", Color::from_raw(0x0000ff) },
    { "Green", Color::from_raw(0x00ff00) }, { "Brown", Color::from_raw(0x800000) },
    { "Purple", Color::from_raw(0x800080) }, { "Gray", Color::from_raw(0x999999) },
    { "Terracotta", Color::from_raw(0xb87f6a) }, { "Silver", Color::from_raw(0xc0c0c0) },
    { "Gold", Color::from_raw(0xd4af37) }, { "Red", Color::from_raw(0xff0000) },
    { "Pink", Color::from_raw(0xff007f) }, { "Orange", Color::from_raw(0xff8000) },
    { "Transparent", Color::from_raw(0xf0f0f0) }, { "Yellow", Color::from_raw(0xffff00) },
    { "White", Color::from_raw(0xffffff) },
}};
#endif
}

std::span<const PaletteProfile> palette() { return profiles; }

std::optional<Profile> custom(const size_t slot) {
    if (slot >= custom_slot_count || !(config_store().custom_filament_color_valid.get() & (1u << slot))) return std::nullopt;
    const auto stored_names = config_store().custom_filament_color_names.get();
    return Profile { stored_names[slot], Color::from_raw(config_store().custom_filament_color_rgb.get(slot)) };
}

bool set_custom(const size_t slot, const std::string_view name, const Color color) {
    if (slot >= custom_slot_count || name.empty() || name.size() >= name_capacity) return false;
    std::array<char, name_capacity> stored {};
    std::copy(name.begin(), name.end(), stored.begin());
    auto names = config_store().custom_filament_color_names.get();
    names[slot] = stored;
    config_store().custom_filament_color_names.set(names);
    config_store().custom_filament_color_rgb.set(slot, color.raw);
    config_store().custom_filament_color_valid.set(config_store().custom_filament_color_valid.get() | (1u << slot));
    return true;
}

std::optional<Color> loaded(const uint8_t tool) {
    if (tool >= VirtualToolIndex::count || !(config_store().loaded_filament_color_valid.get() & (1u << tool))) return std::nullopt;
    return Color::from_raw(config_store().loaded_filament_color_rgb.get(tool));
}

void set_loaded(const uint8_t tool, const std::optional<Color> color) {
    if (tool >= VirtualToolIndex::count) return;
    auto valid = config_store().loaded_filament_color_valid.get();
    if (color) {
        config_store().loaded_filament_color_rgb.set(tool, color->raw);
        valid |= 1u << tool;
    } else {
        valid &= ~(1u << tool);
    }
    config_store().loaded_filament_color_valid.set(valid);
}

std::string_view name_for(const Color color) {
    for (const auto &profile : profiles) if (profile.color == color) return profile.name_view();
    for (size_t slot = 0; slot < custom_slot_count; ++slot) if (const auto profile = custom(slot); profile && profile->color == color) return profile->name_view();
    return "Custom";
}
} // namespace filament_color

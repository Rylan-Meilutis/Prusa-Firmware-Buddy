#include "filament_manufacturer.hpp"

#include <config_store/store_instance.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace filament_manufacturer {
namespace {
constexpr uint8_t custom_id_base = 0x80;
constexpr std::array<const char *, 50> builtin {{
    "Prusa / Prusament", "eSUN", "Polymaker", "SUNLU", "Bambu Lab",
    "Overture", "Atomic Filament", "Hatchbox", "ELEGOO", "Anycubic",
    "Creality", "3D Fuel", "3DJake", "3DXTECH", "Amolen", "AzureFilm",
    "BASF Forward AM", "colorFabb", "Devil Design", "Duramic 3D", "ERYONE",
    "Extrudr", "Fiberlogy", "Fillamentum", "FlashForge", "FormFutura",
    "Inland", "JAYO", "Kexcelled", "MatterHackers", "Proto-pasta",
    "Push Plastic", "QIDI Tech", "Recreus", "ROSA3D", "Spectrum Filaments",
    "AddNorth", "Cookiecad", "COEX 3D", "IC3D", "Jessie", "NinjaTek",
    "Printed Solid", "Kimya", "Nanovia", "ZIRO", "Sovol", "Snapmaker",
    "Voxelab", "Zortrax",
}};

bool equal_ci(const std::string_view a, const std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const char lhs, const char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
    });
}

Profile make_profile(const uint8_t id, const std::string_view name) {
    Profile result { .id = id };
    std::copy_n(name.begin(), std::min(name.size(), name_capacity - 1), result.name.begin());
    return result;
}
}

std::span<const char *const> presets() { return builtin; }

std::optional<Profile> custom(const size_t slot) {
    if (slot >= custom_slot_count || !(config_store().custom_filament_manufacturer_valid.get() & (1u << slot))) return std::nullopt;
    const auto names = config_store().custom_filament_manufacturer_names.get();
    return Profile { custom_id_base + static_cast<uint8_t>(slot), names[slot] };
}

bool set_custom(const size_t slot, const std::string_view name) {
    if (slot >= custom_slot_count || name.empty() || name.size() >= name_capacity || find(name)) return false;
    auto names = config_store().custom_filament_manufacturer_names.get();
    names[slot] = {};
    std::copy(name.begin(), name.end(), names[slot].begin());
    config_store().custom_filament_manufacturer_names.set(names);
    config_store().custom_filament_manufacturer_valid.set(config_store().custom_filament_manufacturer_valid.get() | (1u << slot));
    return true;
}

bool clear_custom(const size_t slot) {
    if (slot >= custom_slot_count) return false;
    config_store().custom_filament_manufacturer_valid.set(config_store().custom_filament_manufacturer_valid.get() & ~(1u << slot));
    return true;
}

std::optional<Profile> find(const std::string_view name) {
    for (size_t i = 0; i < builtin.size(); ++i) if (equal_ci(name, builtin[i])) return make_profile(static_cast<uint8_t>(i + 1), builtin[i]);
    for (size_t i = 0; i < custom_slot_count; ++i) if (const auto item = custom(i); item && equal_ci(name, item->name_view())) return item;
    return std::nullopt;
}

std::optional<Profile> from_id(const uint8_t id) {
    if (id >= 1 && id <= builtin.size()) return make_profile(id, builtin[id - 1]);
    if (id >= custom_id_base && id < custom_id_base + custom_slot_count) return custom(id - custom_id_base);
    return std::nullopt;
}

std::optional<Profile> loaded(const uint8_t tool) {
    if (tool >= EXTRUDERS) return std::nullopt;
    return from_id(config_store().loaded_filament_manufacturer.get(tool));
}

void set_loaded(const uint8_t tool, const std::optional<uint8_t> id) {
    if (tool < EXTRUDERS) config_store().loaded_filament_manufacturer.set(tool, id.value_or(0));
}

} // namespace filament_manufacturer

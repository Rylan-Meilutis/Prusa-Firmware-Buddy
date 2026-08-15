#include "filament_manufacturer.hpp"

#include <config_store/store_instance.hpp>
#include <serial_printing.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace filament_manufacturer {
namespace {
    constexpr uint8_t custom_id_base = 0x80;
    // Packed rather than an array of pointers: the names are immutable and are
    // walked only while opening a picker or resolving a host-provided name. This
    // removes fifty relocatable pointers and gives constant merging one canonical
    // copy of each name across all manufacturer workflows.
    constexpr char builtin[] = "Prusa / Prusament\0eSUN\0Polymaker\0SUNLU\0Bambu Lab\0"
                               "Overture\0Atomic Filament\0Hatchbox\0ELEGOO\0Anycubic\0"
                               "Creality\0"
                               "3D Fuel\0"
                               "3DJake\0"
                               "3DXTECH\0Amolen\0AzureFilm\0"
                               "BASF Forward AM\0colorFabb\0Devil Design\0Duramic 3D\0ERYONE\0"
                               "Extrudr\0Fiberlogy\0Fillamentum\0FlashForge\0FormFutura\0"
                               "Inland\0JAYO\0Kexcelled\0MatterHackers\0Proto-pasta\0"
                               "Push Plastic\0QIDI Tech\0Recreus\0ROSA3D\0Spectrum Filaments\0"
                               "AddNorth\0Cookiecad\0COEX 3D\0IC3D\0Jessie\0NinjaTek\0"
                               "Printed Solid\0Kimya\0Nanovia\0ZIRO\0Sovol\0Snapmaker\0"
                               "Voxelab\0Zortrax\0";

    std::string_view builtin_at(size_t index) {
        if (index >= preset_count) {
            return {};
        }
        const char *name = builtin;
        while (index--) {
            name += std::strlen(name) + 1;
        }
        return name;
    }

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
} // namespace

std::string_view preset(const size_t index) { return builtin_at(index); }

std::optional<Profile> custom(const size_t slot) {
    if (slot >= custom_slot_count || !(config_store().custom_filament_manufacturer_valid.get() & (1u << slot))) {
        return std::nullopt;
    }
    const auto names = config_store().custom_filament_manufacturer_names.get();
    return Profile { custom_id_base + static_cast<uint8_t>(slot), names[slot] };
}

bool set_custom(const size_t slot, const std::string_view name, const bool from_host, const uint32_t transaction) {
    if (slot >= custom_slot_count || name.empty() || name.size() >= name_capacity || find(name)) {
        return false;
    }
    auto names = config_store().custom_filament_manufacturer_names.get();
    names[slot] = {};
    std::copy(name.begin(), name.end(), names[slot].begin());
    config_store().custom_filament_manufacturer_names.set(names);
    config_store().custom_filament_manufacturer_valid.set(config_store().custom_filament_manufacturer_valid.get() | (1u << slot));
    SerialPrinting::notify_configuration("manufacturer", "custom", from_host, transaction);
    return true;
}

bool clear_custom(const size_t slot, const bool from_host, const uint32_t transaction) {
    if (slot >= custom_slot_count) {
        return false;
    }
    const auto valid = config_store().custom_filament_manufacturer_valid.get();
    if (!(valid & (1u << slot))) {
        return true;
    }
    config_store().custom_filament_manufacturer_valid.set(valid & ~(1u << slot));
    SerialPrinting::notify_configuration("manufacturer", "custom", from_host, transaction);
    return true;
}

std::optional<Profile> find(const std::string_view name) {
    for (size_t i = 0; i < preset_count; ++i) {
        if (const auto item = builtin_at(i); equal_ci(name, item)) {
            return make_profile(static_cast<uint8_t>(i + 1), item);
        }
    }
    for (size_t i = 0; i < custom_slot_count; ++i) {
        if (const auto item = custom(i); item && equal_ci(name, item->name_view())) {
            return item;
        }
    }
    return std::nullopt;
}

std::optional<Profile> from_id(const uint8_t id) {
    if (id >= 1 && id <= preset_count) {
        return make_profile(id, builtin_at(id - 1));
    }
    if (id >= custom_id_base && id < custom_id_base + custom_slot_count) {
        return custom(id - custom_id_base);
    }
    return std::nullopt;
}

std::optional<Profile> loaded(const uint8_t tool) {
    if (tool >= EXTRUDERS) {
        return std::nullopt;
    }
    return from_id(config_store().loaded_filament_manufacturer.get(tool));
}

void set_loaded(const uint8_t tool, const std::optional<uint8_t> id, const bool from_host, const uint32_t transaction) {
    if (tool < EXTRUDERS) {
        const uint8_t value = id.value_or(0);
        if (config_store().loaded_filament_manufacturer.get(tool) == value) {
            return;
        }
        config_store().loaded_filament_manufacturer.set(tool, value);
        SerialPrinting::notify_configuration("manufacturer", "loaded", from_host, transaction);
    }
}

} // namespace filament_manufacturer

#include "filament_to_load.hpp"

static FilamentType filament_to_load = FilamentType::none;

FilamentType filament::get_type_to_load() {
    return filament_to_load;
}

void filament::set_type_to_load(FilamentType filament) {
    filament_to_load = filament;
}

static std::optional<Color> color_to_load { std::nullopt };

std::optional<Color> filament::get_color_to_load() {
    return color_to_load;
}

void filament::set_color_to_load(std::optional<Color> color) {
    color_to_load = color;
}

FilamentType filament::apply_load_color(FilamentType filament, VirtualToolIndex virtual_tool, OptionalColor color) {
    // NoFilament must stay NoFilament
    if (!color.has_value() || filament == FilamentType::none) {
        return filament;
    }

    FilamentTypeParameters params = filament.parameters();
    params.color = *color;

    // virtual_tool will matter once pending adhoc filaments become per-tool
    (void)virtual_tool;

    // Pending adhoc -> the color applies to this load only,
    // the user preset stays untouched (and a system preset cannot be changed at all)
    filament = FilamentType::pending_adhoc;
    filament.set_parameters(params);
    return filament;
}

static std::optional<uint8_t> manufacturer_to_load { std::nullopt };

std::optional<uint8_t> filament::get_manufacturer_to_load() { return manufacturer_to_load; }
void filament::set_manufacturer_to_load(std::optional<uint8_t> manufacturer) { manufacturer_to_load = manufacturer; }

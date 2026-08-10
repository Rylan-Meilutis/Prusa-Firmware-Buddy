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

static std::optional<uint8_t> manufacturer_to_load { std::nullopt };

std::optional<uint8_t> filament::get_manufacturer_to_load() { return manufacturer_to_load; }
void filament::set_manufacturer_to_load(std::optional<uint8_t> manufacturer) { manufacturer_to_load = manufacturer; }

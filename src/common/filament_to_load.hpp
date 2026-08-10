#pragma once

#include <filament.hpp>
#include <color.hpp>

// TODO: Remove this ugly interface
namespace filament {

FilamentType get_type_to_load();
void set_type_to_load(FilamentType filament);

std::optional<Color> get_color_to_load();
void set_color_to_load(std::optional<Color> color);
std::optional<uint8_t> get_manufacturer_to_load();
void set_manufacturer_to_load(std::optional<uint8_t> manufacturer);

} // namespace filament

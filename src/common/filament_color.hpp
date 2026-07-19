#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <utils/color.hpp>

namespace filament_color {

constexpr size_t custom_slot_count = 8;
constexpr size_t name_capacity = 16;

struct Profile {
    std::string_view name;
    Color color;
};

const std::array<Profile, 15> &palette();
std::optional<Profile> custom(size_t slot);
bool set_custom(size_t slot, std::string_view name, Color color);
std::optional<Color> loaded(uint8_t tool);
void set_loaded(uint8_t tool, std::optional<Color> color);
std::string_view name_for(Color color);

} // namespace filament_color

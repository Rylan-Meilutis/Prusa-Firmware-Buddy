#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace filament_manufacturer {

constexpr size_t custom_slot_count = 8;
constexpr size_t name_capacity = 24;

struct Profile {
    uint8_t id;
    std::array<char, name_capacity> name {};
    std::string_view name_view() const { return name.data(); }
};

std::span<const char *const> presets();
std::optional<Profile> custom(size_t slot);
bool set_custom(size_t slot, std::string_view name);
bool clear_custom(size_t slot);
std::optional<Profile> find(std::string_view name);
std::optional<Profile> from_id(uint8_t id);
std::optional<Profile> loaded(uint8_t tool);
void set_loaded(uint8_t tool, std::optional<uint8_t> id);

} // namespace filament_manufacturer

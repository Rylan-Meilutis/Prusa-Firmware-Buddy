#pragma once

#include <optional>

#include <filament.hpp>
#include <tool_index.hpp>
#include <color.hpp>
#include <utils/compact_optional.hpp>
#include <utils/storage/strong_index_array.hpp>
#include <gcode_basic_parser.hpp>
#include <general_response.hpp>
#include <feature/compatibility_checks/compatibility_checks_common.hpp>

namespace multi_filament_change {

enum class Action : uint8_t {
    /// Keep as is, do not change the filament
    keep,

    /// Change filament to \p new_filament
    change,

    /// Unload the filament
    unload,
};

struct ConfigItem {
    Action action = Action::keep;
    EncodedFilamentType new_filament = FilamentType::none;
    uint8_t manufacturer = 0;
    [[nodiscard]] bool gui_confirm_incompatibilities(std::variant<VirtualToolIndex, AllTools> tools, Response abort_response, buddy::compatibility_checks::CompatibilityLevel skip_level = buddy::compatibility_checks::CompatibilityLevel::fully_compatible) const;
};

using ConfigItems = StrongIndexArray<ConfigItem, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static>;
using ConfigColors = StrongIndexArray<CompactOptional<Color, COLOR_NONE>, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static>;

// Keep colors separate to avoid padding each serialized slot.
struct Config {
    ConfigItems items {};
    ConfigColors colors {};
    ConfigItem &operator[](VirtualToolIndex tool) { return items[tool]; }
    const ConfigItem &operator[](VirtualToolIndex tool) const { return items[tool]; }
    auto begin() { return items.begin(); }
    auto end() { return items.end(); }
    auto begin() const { return items.begin(); }
    auto end() const { return items.end(); }
    static Config from_current_print_setup();
    static std::optional<Config> from_gcode(GCodeBasicParser &parser);
    void to_gcode(StringBuilder &sb) const;
    void set_all_loaded_to_unload();
    void execute() const;
    [[nodiscard]] bool gui_confirm_incompatibilities(Response abort_response, buddy::compatibility_checks::CompatibilityLevel skip_level = buddy::compatibility_checks::CompatibilityLevel::fully_compatible) const;
};

/// GCode command used to represent the gcode
inline constexpr GCodeCommand gcode_command {
    .letter = 'M',
    .codenum = 9934,
};

} // namespace multi_filament_change

/// Configuration used in DialogChangeAllFilaments
using MultiFilamentChangeConfig = multi_filament_change::Config;

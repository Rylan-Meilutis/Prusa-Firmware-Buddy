#include "multi_filament_change.hpp"

#include <gcode_info.hpp>
#include <module/prusa/spool_join.hpp>
#include <module/prusa/tool_mapper.hpp>

namespace multi_filament_change {

Config config_from_current_print_setup() {
    Config result;

    const auto &gcode_info = GCodeInfo::getInstance();

    for (const auto virtual_tool : VirtualToolIndex::all()) {
        const auto main_virtual_tool = spool_join.get_first_spool_1_from_chain(virtual_tool);
        const auto gcode_tool = stdext::get_optional<GcodeToolIndex>(tool_mapper.to_gcode(main_virtual_tool));

        // Not assigned -> keep as 'don't change'
        if (!gcode_tool.has_value()) {
            continue;
        }

        const auto &tool_info = gcode_info.get_extruder_info(*gcode_tool);

        auto &item = result[virtual_tool];

        assert(tool_info.used()); // otherwise bug in mapping
        item.color = tool_info.extruder_colour;

        const auto &opt_name = tool_info.filament_name;
        if (opt_name.empty()) {
            continue;
        }

        // Only preselect if we don't have it already
        if (config_store().get_filament_type(virtual_tool).matches(opt_name)) {
            continue;
        }

        item.action = multi_filament_change::Action::change;

        // We're loading a new filament, do not fallback into ad-hoc one -> extruder_index = std::nullopt
        item.new_filament = FilamentType::from_name(opt_name);
    }

    return result;
}

} // namespace multi_filament_change

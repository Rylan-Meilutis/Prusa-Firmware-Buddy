#include "toolchanger_utils.h"
#include <serial_printing.hpp>
#include "tool_offset.hpp"
#include "dock_position.hpp"
#include <tool_index.hpp>

#include <option/has_indx.h>
#include <option/has_toolchanger.h>

#include "Marlin/src/module/stepper.h"
#include "Marlin/src/module/motion.h"
#include "Marlin.h"
#include "timing.h"
#include <tool/hotend/hotend/indx_hotend.hpp>
#include <utils/badge.hpp>

#include <config_store/store_instance.hpp>
#include <indx_dock_tolerance.hpp>

bool PrusaToolChangerUtils::is_pos_in_toolchange_area(const xy_pos_t &pos) {
    return pos.y < SAFE_Y_WITH_TOOL;
}

void PrusaToolChangerUtils::set_active_extruder(std::variant<PhysicalToolIndex, NoTool> maybe_tool) {
    // Pickup/park flip heating earlier for finer timing.
    // Boot, PP resume, and dropped-nozzle recovery skip pickup/park, we have to handle the heating transition here too.
    match(
        maybe_tool,
        [&](PhysicalToolIndex tool) {
            active_extruder = tool.to_raw();
            // any active-tool change reapplies the matching offset
            hotend_currently_applied_offset = hotend_offset[tool];

            IndxHotend &to_be_active_hotend = IndxHotend::indx_tool(tool).hotend();
            if (!to_be_active_hotend.is_thermally_managed()) {
                to_be_active_hotend.start_heating();
            }
        },
        [&](NoTool) {
            // order is important here: currently_selected() reads active_extruder
            if (auto curr = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::currently_selected())) {
                auto &h = IndxHotend::indx_tool(*curr).hotend();
                if (h.is_thermally_managed()) {
                    h.stop_heating();
                }
            }
            active_extruder = MARLIN_NO_TOOL_PICKED;
            hotend_currently_applied_offset = xyz_pos_t {};
        });

    IndxHotend::assert_thermally_managed_invariant(maybe_tool);
}

void PrusaToolChangerUtils::restore_last_picked_tool() {
    // INDX has no sensors; trust EEPROM for the last picked tool
    if (config_store().indx_last_picked_tool_valid.get()) {
        set_active_extruder(config_store().get_indx_last_picked_tool());
    } else {
        set_active_extruder(NoTool {});
    }
}

bool PrusaToolChangerUtils::is_tool_enabled(PhysicalToolIndex tool, Badge<PhysicalToolIndex>) {
    return config_store().indx_dock_calibrated_mask.get().test(tool.to_raw());
}

uint8_t PrusaToolChangerUtils::get_num_enabled_tools() const {
    return config_store().indx_dock_calibrated_mask.get().count();
}

const PrusaToolInfo &PrusaToolChangerUtils::get_tool_info(PhysicalToolIndex tool_index, bool check_calibrated) const {
    const PrusaToolInfo &info = tool_info[tool_index];
    if (check_calibrated && !is_tool_info_valid(tool_index, info)) {
        toolchanger_error("Dock Position not calibrated");
    }
    return info;
}

bool PrusaToolChangerUtils::is_tool_info_valid(PhysicalToolIndex tool, const PrusaToolInfo &info) const {
    const PrusaToolInfo synthetic = create_default_tool_info(tool);
    const float tolerance_x = indx_dock_tolerance::sanitize(config_store().indx_dock_tolerance_x_mm.get(), indx_dock_tolerance::default_x_mm);
    const float tolerance_y = indx_dock_tolerance::sanitize(config_store().indx_dock_tolerance_y_mm.get(), indx_dock_tolerance::default_y_mm);
    return indx_dock_tolerance::accepts_offset(info.dock_x - synthetic.dock_x, info.dock_y - synthetic.dock_y, tolerance_x, tolerance_y, EPSILON_MM);
}

PrusaToolInfo PrusaToolChangerUtils::create_default_tool_info(PhysicalToolIndex tool) const {
    return PrusaToolInfo({ .dock_x = DOCK_DEFAULT_X_MM[tool],
        .dock_y = DOCK_DEFAULT_Y_MM });
}

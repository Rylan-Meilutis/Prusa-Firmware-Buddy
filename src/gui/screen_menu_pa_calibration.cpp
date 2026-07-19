#include "screen_menu_pa_calibration.hpp"

#include <DialogHandler.hpp>
#include <config_store/store_instance.hpp>
#include <filament.hpp>
#include <marlin_client.hpp>
#include <option/has_mmu2.h>
#include <tool_index.hpp>

#include <array>
#include <cstdio>

namespace {
constexpr std::array<const char *, 6> tool_names { N_("Tool 1"), N_("Tool 2"), N_("Tool 3"), N_("Tool 4"), N_("Tool 5"), N_("Tool 6") };
uint8_t selected_tool = 0;
bool sequential = true;
uint16_t temperature_override = 0;

constexpr NumericInputConfig temperature_config {
    .min_value = 170,
    .max_value = 300,
    .step = 5,
    .special_value = 0,
    .special_value_str = N_("Material"),
};

bool configured(VirtualToolIndex tool) {
    return tool.is_enabled() && config_store().get_filament_type(tool) != FilamentType::none;
}

size_t configured_count() {
    size_t count = 0;
    for (auto tool : VirtualToolIndex::all()) count += configured(tool);
    return count;
}

uint8_t first_configured() {
    for (auto tool : VirtualToolIndex::all()) if (configured(tool)) return tool.to_raw();
    return 0;
}

uint8_t initial_tool() {
    selected_tool = first_configured();
    return selected_tool;
}

void submit(VirtualToolIndex logical) {
    const auto filament_type = config_store().get_filament_type(logical);
    if (filament_type == FilamentType::none) return;
    const auto &params = filament_type.parameters();
    const int temperature = temperature_override ? temperature_override : params.nozzle_temperature;
#if HAS_MMU2()
    constexpr unsigned physical = 0;
#else
    const unsigned physical = logical.to_physical().to_raw();
#endif
    char command[MARLIN_MAX_REQUEST + 1];
    snprintf(command, sizeof(command), "M976 C L%u", logical.to_raw());
    marlin_client::gcode(command);
    snprintf(command, sizeof(command), "M976 A %u:%u:%s:%d", physical, logical.to_raw(), params.name.data(), temperature);
    marlin_client::gcode(command);
}
} // namespace

MI_PA_TOOL::MI_PA_TOOL()
    : MenuItemSwitch(_("Tool / MMU Slot"), tool_names, initial_tool()) {
    set_is_hidden(configured_count() <= 1 ? is_hidden_t::yes : is_hidden_t::no);
}

void MI_PA_TOOL::OnChange(size_t) {
    selected_tool = get_index();
}

MI_PA_SEQUENTIAL::MI_PA_SEQUENTIAL()
    : WI_ICON_SWITCH_OFF_ON_t(sequential, _("Calibrate All Sequentially"), nullptr, is_enabled_t::yes,
        configured_count() > 1 ? is_hidden_t::no : is_hidden_t::yes) {}

void MI_PA_SEQUENTIAL::OnChange(size_t old_index) {
    sequential = !old_index;
}

MI_PA_TEMPERATURE::MI_PA_TEMPERATURE()
    : WiSpin(temperature_override, temperature_config, _("Test Temperature")) {}

void MI_PA_TEMPERATURE::OnClick() {
    temperature_override = static_cast<uint16_t>(value());
}

MI_PA_RUN::MI_PA_RUN()
    : IWindowMenuItem(_("Run PA Calibration"), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_PA_RUN::click(IWindowMenu &) {
    if (configured_count() == 0) {
        MsgBoxWarning(_("Assign the loaded filament material before calibration."), Responses_Ok);
        return;
    }
    if (MsgBoxQuestion(_("Remove all objects and filament debris from the build and front service areas before continuing."), Responses_ContinueAbort) != Response::Continue) {
        return;
    }
    if (sequential && configured_count() > 1) {
        for (auto tool : VirtualToolIndex::all()) if (configured(tool)) submit(tool);
    } else {
        const auto tool = VirtualToolIndex::from_raw(selected_tool);
        if (!configured(tool)) {
            MsgBoxWarning(_("The selected tool or MMU slot has no assigned filament."), Responses_Ok);
            return;
        }
        submit(tool);
    }
}

ScreenMenuPACalibration::ScreenMenuPACalibration()
    : ScreenMenuPACalibration_(_("PRESSURE ADVANCE")) {}

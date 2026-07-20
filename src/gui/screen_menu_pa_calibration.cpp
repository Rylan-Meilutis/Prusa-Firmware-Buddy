#include "screen_menu_pa_calibration.hpp"

#include <DialogHandler.hpp>
#include <config_store/store_instance.hpp>
#include <filament.hpp>
#include <feature/extrusion_calibration.hpp>
#include <marlin_client.hpp>
#include <option/has_mmu2.h>
#include <print_utils.hpp>

#include <array>
#include <algorithm>
#include <cstdio>

#if HAS_PA_CALIBRATION_UI()

namespace {
constexpr std::array<const char *, 8> tool_names { N_("Tool 1 Calibration"), N_("Tool 2 Calibration"), N_("Tool 3 Calibration"), N_("Tool 4 Calibration"), N_("Tool 5 Calibration"), N_("Tool 6 Calibration"), N_("Tool 7 Calibration"), N_("Tool 8 Calibration") };
constexpr std::array<const char *, 8> temperature_names { N_("Tool 1 Temperature"), N_("Tool 2 Temperature"), N_("Tool 3 Temperature"), N_("Tool 4 Temperature"), N_("Tool 5 Temperature"), N_("Tool 6 Temperature"), N_("Tool 7 Temperature"), N_("Tool 8 Temperature") };
std::array<uint16_t, buddy::extrusion_calibration::max_logical_filaments> temperature_overrides {};
std::array<bool, buddy::extrusion_calibration::max_logical_filaments> selected_tools {};

constexpr NumericInputConfig temperature_config {
    .min_value = 170,
    .max_value = 300,
    .step = 5,
    .special_value = 0,
    .special_value_str = N_("Material"),
};

bool configured(const uint8_t tool) {
    return is_tool_enabled(tool) && config_store().get_filament_type(tool) != FilamentType::none;
}

size_t configured_count() {
    size_t count = 0;
    for (uint8_t tool = 0; tool < buddy::extrusion_calibration::max_logical_filaments; ++tool) count += configured(tool);
    return count;
}

void submit_selected() {
    uint8_t mask = 0;
    for (uint8_t tool = 0; tool < buddy::extrusion_calibration::max_logical_filaments; ++tool) {
        if (configured(tool) && selected_tools[tool]) mask |= 1u << tool;
    }
    char command[MARLIN_MAX_REQUEST + 1];
    snprintf(command, sizeof(command), "M976 M K%u U%u,%u,%u,%u,%u,%u,%u,%u", unsigned(mask),
        unsigned(temperature_overrides[0]), unsigned(temperature_overrides[1]),
        unsigned(temperature_overrides[2]), unsigned(temperature_overrides[3]),
        unsigned(temperature_overrides[4]), unsigned(temperature_overrides[5]),
        unsigned(temperature_overrides[6]), unsigned(temperature_overrides[7]));
    marlin_client::gcode(command);
}

bool confirm_clean_area() {
    return MsgBoxQuestion(string_view_utf8::MakeCPUFLASH("Clear the build and front service areas before continuing."), Responses_ContinueAbort) == Response::Continue;
}
} // namespace

MI_PA_TOOL_RUN::MI_PA_TOOL_RUN(const uint8_t tool)
    : WI_ICON_SWITCH_OFF_ON_t(configured(tool), _(tool_names[tool]), nullptr,
        is_enabled_t::yes, configured(tool) ? is_hidden_t::no : is_hidden_t::yes)
    , tool_(tool) {
    selected_tools[tool] = configured(tool);
}

void MI_PA_TOOL_RUN::OnChange(size_t) {
    selected_tools[tool_] = value();
}

MI_PA_TEMPERATURE::MI_PA_TEMPERATURE(const uint8_t tool)
    : WiSpin(temperature_overrides[tool], temperature_config, _(temperature_names[tool]), nullptr,
        is_enabled_t::yes, configured(tool) ? is_hidden_t::no : is_hidden_t::yes)
    , tool_(tool) {}

void MI_PA_TEMPERATURE::OnClick() {
    temperature_overrides[tool_] = static_cast<uint16_t>(value());
}

MI_PA_RUN::MI_PA_RUN()
    : IWindowMenuItem(_("Run Selected Calibrations"), nullptr, is_enabled_t::yes,
        configured_count() > 0 ? is_hidden_t::no : is_hidden_t::yes) {}

void MI_PA_RUN::click(IWindowMenu &) {
    if (configured_count() == 0) {
        MsgBoxWarning(string_view_utf8::MakeCPUFLASH("Assign the loaded filament before calibration."), Responses_Ok);
        return;
    }
    bool any_selected = false;
    for (uint8_t tool = 0; tool < buddy::extrusion_calibration::max_logical_filaments; ++tool) any_selected |= configured(tool) && selected_tools[tool];
    if (!any_selected) {
        MsgBoxWarning(string_view_utf8::MakeCPUFLASH("Select at least one loaded filament."), Responses_Ok);
        return;
    }
    if (!confirm_clean_area()) return;
    submit_selected();
}

ScreenMenuPACalibration::ScreenMenuPACalibration()
    : ScreenMenuPACalibration_(_("PRESSURE ADVANCE")) {}

#endif // HAS_PA_CALIBRATION_UI()

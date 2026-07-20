#include "screen_menu_pa_calibration.hpp"

#include <DialogHandler.hpp>
#include <config_store/store_instance.hpp>
#include <filament.hpp>
#include <feature/extrusion_calibration.hpp>
#include <marlin_client.hpp>
#include <option/has_mmu2.h>
#include <print_utils.hpp>
#include <tool_index.hpp>

#include <array>
#include <algorithm>
#include <cstdio>

#if HAS_PA_CALIBRATION_UI()

namespace {
constexpr std::array<const char *, 6> tool_names { N_("Tool 1 Calibration"), N_("Tool 2 Calibration"), N_("Tool 3 Calibration"), N_("Tool 4 Calibration"), N_("Tool 5 Calibration"), N_("Tool 6 Calibration") };
uint16_t temperature_override = 0;

constexpr NumericInputConfig temperature_config {
    .min_value = 170,
    .max_value = 300,
    .step = 5,
    .special_value = 0,
    .special_value_str = N_("Material"),
};

bool configured(const VirtualToolIndex tool) {
    return tool.is_enabled() && config_store().get_filament_type(tool) != FilamentType::none;
}

size_t configured_count() {
    size_t count = 0;
    for (auto tool : VirtualToolIndex::all()) count += configured(tool);
    return count;
}

void submit(const VirtualToolIndex logical) {
    const auto filament_type = config_store().get_filament_type(logical);
    if (filament_type == FilamentType::none) return;
    const auto &params = filament_type.parameters();
    // A manual override is intentionally constrained around the loaded
    // material profile. This prevents a stale value selected for one tool
    // from becoming unsafe when another material/tool is calibrated.
    constexpr int material_temperature_margin = 15;
    constexpr int machine_minimum = 170;
    constexpr int machine_maximum = 300;
    const int profile_temperature = std::clamp<int>(params.nozzle_temperature, machine_minimum, machine_maximum);
    const int temperature = temperature_override
        ? std::clamp<int>(temperature_override,
            std::max(machine_minimum, profile_temperature - material_temperature_margin),
            std::min(machine_maximum, profile_temperature + material_temperature_margin))
        : profile_temperature;
#if HAS_MMU2()
    constexpr unsigned physical = 0;
#else
    const unsigned physical = logical.to_physical().to_raw();
#endif
    char command[MARLIN_MAX_REQUEST + 1];
    snprintf(command, sizeof(command), "M976 C L%u", logical.to_raw());
    marlin_client::gcode(command);
    snprintf(command, sizeof(command), "M976 M A %u:%u:%s:%d", physical, logical.to_raw(), params.name.data(), temperature);
    marlin_client::gcode(command);
}

bool confirm_clean_area() {
    return MsgBoxQuestion(string_view_utf8::MakeCPUFLASH("Clear the build and front service areas before continuing."), Responses_ContinueAbort) == Response::Continue;
}
} // namespace

MI_PA_TOOL_RUN::MI_PA_TOOL_RUN(const uint8_t tool)
    : IWindowMenuItem(_(tool_names[tool]), nullptr, is_enabled_t::yes, configured(VirtualToolIndex::from_raw(tool)) ? is_hidden_t::no : is_hidden_t::yes, expands_t::yes)
    , tool_(tool) {
}

void MI_PA_TOOL_RUN::click(IWindowMenu &) {
    const auto tool = VirtualToolIndex::from_raw(tool_);
    if (configured(tool) && confirm_clean_area()) submit(tool);
}

MI_PA_TEMPERATURE::MI_PA_TEMPERATURE()
    : WiSpin(temperature_override, temperature_config, _("Test Temperature")) {}

void MI_PA_TEMPERATURE::OnClick() {
    temperature_override = static_cast<uint16_t>(value());
}

MI_PA_RUN::MI_PA_RUN()
    : IWindowMenuItem(_("Calibrate All Loaded"), nullptr, is_enabled_t::yes,
        configured_count() > 1 ? is_hidden_t::no : is_hidden_t::yes) {}

void MI_PA_RUN::click(IWindowMenu &) {
    if (configured_count() == 0) {
        MsgBoxWarning(string_view_utf8::MakeCPUFLASH("Assign the loaded filament before calibration."), Responses_Ok);
        return;
    }
    if (!confirm_clean_area()) return;
    for (auto tool : VirtualToolIndex::all()) if (configured(tool)) submit(tool);
}

ScreenMenuPACalibration::ScreenMenuPACalibration()
    : ScreenMenuPACalibration_(_("PRESSURE ADVANCE")) {}

#endif // HAS_PA_CALIBRATION_UI()

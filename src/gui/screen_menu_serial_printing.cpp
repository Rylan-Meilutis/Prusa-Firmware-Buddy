#include "screen_menu_serial_printing.hpp"

#include <config_store/store_instance.hpp>
#include <numeric_input_config.hpp>

namespace {
constexpr NumericInputConfig timeout_config {
    .min_value = 1,
    .max_value = 120,
    .step = 1,
    .unit = Unit::second,
};
} // namespace

MI_SERIAL_PRINT_SCREEN_ENABLE::MI_SERIAL_PRINT_SCREEN_ENABLE()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().serial_print_screen_enabled.get(), _("Serial Printing Screen")) {}

void MI_SERIAL_PRINT_SCREEN_ENABLE::OnChange(size_t old_index) {
    config_store().serial_print_screen_enabled.set(!old_index);
}

MI_SERIAL_PRINT_LEGACY_UI::MI_SERIAL_PRINT_LEGACY_UI()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().serial_print_legacy_ui.get(), _("Legacy Serial UI")) {}

void MI_SERIAL_PRINT_LEGACY_UI::OnChange(size_t old_index) {
    config_store().serial_print_legacy_ui.set(!old_index);
}

MI_SERIAL_PRINT_AUTO_START::MI_SERIAL_PRINT_AUTO_START()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().serial_print_auto_start.get(), _("Auto-detect Print Start")) {}

void MI_SERIAL_PRINT_AUTO_START::OnChange(size_t old_index) {
    config_store().serial_print_auto_start.set(!old_index);
}

MI_SERIAL_PRINT_TIMEOUT::MI_SERIAL_PRINT_TIMEOUT()
    : WiSpin(config_store().serial_print_timeout_s.get(), timeout_config, _("Serial Timeout")) {}

void MI_SERIAL_PRINT_TIMEOUT::OnClick() {
    config_store().serial_print_timeout_s.set(static_cast<uint16_t>(value()));
}

ScreenMenuSerialPrinting::ScreenMenuSerialPrinting()
    : ScreenMenuSerialPrinting__(_("SERIAL PRINTING")) {}

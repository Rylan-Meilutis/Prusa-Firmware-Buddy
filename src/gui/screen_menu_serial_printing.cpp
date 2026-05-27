#include "screen_menu_serial_printing.hpp"

#include <config_store/store_instance.hpp>
#include <numeric_input_config.hpp>
#include <serial_printing.hpp>

namespace {
constexpr NumericInputConfig timeout_config {
    .min_value = 1,
    .max_value = 120,
    .step = 1,
    .unit = Unit::second,
};

static constexpr const char *serial_print_ui_mode_items[] {
    N_("Legacy"),
    N_("Messages Only"),
    N_("Progress"),
};
} // namespace

MI_SERIAL_PRINT_UI_MODE::MI_SERIAL_PRINT_UI_MODE()
    : MenuItemSwitch(_("Serial Printing UI"), serial_print_ui_mode_items, static_cast<size_t>(SerialPrinting::ui_mode())) {}

void MI_SERIAL_PRINT_UI_MODE::OnChange([[maybe_unused]] size_t old_index) {
    const auto mode = static_cast<SerialPrintingUiMode>(get_index());
    config_store().serial_print_ui_mode.set(mode);
    config_store().serial_print_legacy_ui.set(mode == SerialPrintingUiMode::legacy);
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

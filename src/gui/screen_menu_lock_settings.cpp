#include "screen_menu_lock_settings.hpp"

#include <config_store/store_instance.hpp>
#include <dialogs/dialog_text_input.hpp>
#include <numeric_input_config.hpp>
#include <array>

namespace {
constexpr NumericInputConfig timeout_config {
    .min_value = 15,
    .max_value = 3600,
    .step = 15,
    .unit = Unit::second,
};

bool parse_pin(const std::array<char, 7> &buffer, uint32_t &pin, uint8_t &length) {
    pin = 0;
    length = 0;
    for (char ch : buffer) {
        if (ch == '\0') {
            break;
        }
        if (ch < '0' || ch > '9') {
            return false;
        }
        pin = pin * 10 + static_cast<uint32_t>(ch - '0');
        ++length;
    }
    return length >= 4 && length <= 6;
}

bool set_unlock_code() {
    std::array<char, 7> buffer {};
    uint32_t pin = 0;
    uint8_t length = 0;
    if (!DialogTextInput::exec(_("Unlock Code"), buffer, true, true) || !parse_pin(buffer, pin, length)) {
        return false;
    }
    config_store().printer_lock_pin.set(pin);
    config_store().printer_lock_pin_length.set(length);
    return true;
}
} // namespace

MI_PRINTER_LOCK_ENABLE::MI_PRINTER_LOCK_ENABLE()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().printer_lock_enabled.get(), _("Enable Lock")) {}

void MI_PRINTER_LOCK_ENABLE::OnChange(size_t old_index) {
    const bool enable = !old_index;
    if (enable && config_store().printer_lock_pin_length.get() < 4 && !set_unlock_code()) {
        set_value(false);
        config_store().printer_lock_enabled.set(false);
        return;
    }
    config_store().printer_lock_enabled.set(enable);
}

MI_PRINTER_LOCK_PIN::MI_PRINTER_LOCK_PIN()
    : IWindowMenuItem(_("Set Unlock Code"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_PRINTER_LOCK_PIN::click(IWindowMenu &) {
    set_unlock_code();
}

MI_PRINTER_LOCK_TIMEOUT::MI_PRINTER_LOCK_TIMEOUT()
    : WiSpin(config_store().printer_lock_timeout_s.get(), timeout_config, _("Lock Timeout")) {}

void MI_PRINTER_LOCK_TIMEOUT::OnClick() {
    config_store().printer_lock_timeout_s.set(static_cast<uint16_t>(value()));
}

MI_PRINTER_LOCK_SERIAL::MI_PRINTER_LOCK_SERIAL()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().printer_lock_accept_serial.get(), _("Accept Serial When Locked")) {}

void MI_PRINTER_LOCK_SERIAL::OnChange(size_t old_index) {
    config_store().printer_lock_accept_serial.set(!old_index);
}

ScreenMenuLockSettings::ScreenMenuLockSettings()
    : ScreenMenuLockSettings__(_("PRINTER LOCK")) {}

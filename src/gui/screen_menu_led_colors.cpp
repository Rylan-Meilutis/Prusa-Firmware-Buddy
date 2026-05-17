#include "screen_menu_led_colors.hpp"

#include <config_store/store_instance.hpp>
#include <dialogs/dialog_text_input.hpp>
#include <option/has_leds.h>
#include <utils/string_builder.hpp>
#include <array>
#include <charconv>

#if HAS_LEDS()
    #include <leds/status_leds_handler.hpp>
#endif

namespace {
bool edit_color(const string_view_utf8 &prompt, auto &store_item) {
    std::array<char, 8> buffer {};
    StringBuilder sb(buffer);
    sb.append_printf("%06lx", static_cast<unsigned long>(store_item.get() & 0xffffff));
    if (!DialogTextInput::exec(prompt, buffer)) {
        return false;
    }

    uint32_t raw = 0;
    const char *begin = buffer.data();
    if (*begin == '#') {
        ++begin;
    }
    const char *end = begin;
    while (*end != '\0') {
        ++end;
    }
    if (std::from_chars(begin, end, raw, 16).ec != std::errc {} || raw > 0xffffff) {
        return false;
    }

    store_item.set(raw);
    return true;
}

void reload_status_led_colors() {
#if HAS_LEDS()
    leds::StatusLedsHandler::instance().reload_colors();
#endif
}
} // namespace

MI_UI_THEME_PRIMARY_COLOR::MI_UI_THEME_PRIMARY_COLOR()
    : IWindowMenuItem(_("Primary Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRIMARY_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().ui_theme_primary_color); }

MI_UI_THEME_PROGRESS_COLOR::MI_UI_THEME_PROGRESS_COLOR()
    : IWindowMenuItem(_("Progress Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PROGRESS_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().ui_theme_progress_color); }

MI_UI_THEME_WARNING_COLOR::MI_UI_THEME_WARNING_COLOR()
    : IWindowMenuItem(_("Warning Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_WARNING_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().ui_theme_warning_color); }

MI_UI_THEME_ERROR_COLOR::MI_UI_THEME_ERROR_COLOR()
    : IWindowMenuItem(_("Error Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_ERROR_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().ui_theme_error_color); }

ScreenMenuUiThemeColors::ScreenMenuUiThemeColors()
    : ScreenMenuUiThemeColors__(_("UI THEME COLORS")) {}

MI_STATUS_LED_IDLE_COLOR::MI_STATUS_LED_IDLE_COLOR()
    : IWindowMenuItem(_("Idle Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_STATUS_LED_IDLE_COLOR::click(IWindowMenu &) {
    if (edit_color(GetLabel(), config_store().status_led_idle_color)) {
        reload_status_led_colors();
    }
}

MI_STATUS_LED_PRINTING_COLOR::MI_STATUS_LED_PRINTING_COLOR()
    : IWindowMenuItem(_("Printing Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_STATUS_LED_PRINTING_COLOR::click(IWindowMenu &) {
    if (edit_color(GetLabel(), config_store().status_led_printing_color)) {
        reload_status_led_colors();
    }
}

MI_STATUS_LED_FINISHED_COLOR::MI_STATUS_LED_FINISHED_COLOR()
    : IWindowMenuItem(_("Finished Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_STATUS_LED_FINISHED_COLOR::click(IWindowMenu &) {
    if (edit_color(GetLabel(), config_store().status_led_finished_color)) {
        reload_status_led_colors();
    }
}

MI_STATUS_LED_WARNING_COLOR::MI_STATUS_LED_WARNING_COLOR()
    : IWindowMenuItem(_("Warning Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_STATUS_LED_WARNING_COLOR::click(IWindowMenu &) {
    if (edit_color(GetLabel(), config_store().status_led_warning_color)) {
        reload_status_led_colors();
    }
}

MI_STATUS_LED_ERROR_COLOR::MI_STATUS_LED_ERROR_COLOR()
    : IWindowMenuItem(_("Error Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_STATUS_LED_ERROR_COLOR::click(IWindowMenu &) {
    if (edit_color(GetLabel(), config_store().status_led_error_color)) {
        reload_status_led_colors();
    }
}

ScreenMenuStatusLedColors::ScreenMenuStatusLedColors()
    : ScreenMenuStatusLedColors__(_("STATUS LED COLORS")) {}

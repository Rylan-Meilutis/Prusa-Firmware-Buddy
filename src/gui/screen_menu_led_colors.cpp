#include "screen_menu_led_colors.hpp"

#include "ScreenHandler.hpp"
#include "screen_theme_filebrowser.hpp"
#include <config_store/store_instance.hpp>
#include <dialogs/dialog_text_input.hpp>
#include <option/has_leds.h>
#include <utils/string_builder.hpp>
#include <window_msgbox.hpp>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>

#if HAS_LEDS()
    #include <leds/status_leds_handler.hpp>
#endif

namespace {
struct ThemeColors {
    uint32_t primary;
    uint32_t progress;
    uint32_t warning;
    uint32_t error;
    uint32_t image;
    uint32_t led_idle;
    uint32_t led_printing;
    uint32_t led_finished;
    uint32_t led_warning;
    uint32_t led_error;
};

constexpr ThemeColors theme_prusa {
    .primary = 0xff6b00,
    .progress = 0xff6b00,
    .warning = 0xffcc00,
    .error = 0xff2f2f,
    .image = 0xff6b00,
    .led_idle = 0x000000,
    .led_printing = 0xff6b00,
    .led_finished = 0x00ff00,
    .led_warning = 0xffcc00,
    .led_error = 0xff0000,
};

constexpr ThemeColors theme_default {
    .primary = 0x4b2e83,
    .progress = 0x4b2e83,
    .warning = 0xffff00,
    .error = 0xff0000,
    .image = 0x4b2e83,
    .led_idle = 0x000000,
    .led_printing = 0x0096ff,
    .led_finished = 0x00ff00,
    .led_warning = 0xffff00,
    .led_error = 0xff0000,
};

constexpr ThemeColors theme_blue {
    .primary = 0x005fbd,
    .progress = 0x00a3ff,
    .warning = 0xffd24a,
    .error = 0xff4055,
    .image = 0x00a3ff,
    .led_idle = 0x000000,
    .led_printing = 0x008cff,
    .led_finished = 0x23d18b,
    .led_warning = 0xffd24a,
    .led_error = 0xff4055,
};

constexpr ThemeColors theme_graphite {
    .primary = 0x3f4754,
    .progress = 0x8fb3ff,
    .warning = 0xf6c343,
    .error = 0xf25f5c,
    .image = 0x8fb3ff,
    .led_idle = 0x000000,
    .led_printing = 0x8fb3ff,
    .led_finished = 0x74d680,
    .led_warning = 0xf6c343,
    .led_error = 0xf25f5c,
};

constexpr ThemeColors theme_forest {
    .primary = 0x1f6f4a,
    .progress = 0x33c27f,
    .warning = 0xf0c94a,
    .error = 0xd94343,
    .image = 0x33c27f,
    .led_idle = 0x000000,
    .led_printing = 0x28b36b,
    .led_finished = 0x6cff8f,
    .led_warning = 0xf0c94a,
    .led_error = 0xd94343,
};

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

void apply_theme(const ThemeColors &theme) {
    config_store().ui_theme_primary_color.set(theme.primary);
    config_store().ui_theme_progress_color.set(theme.progress);
    config_store().ui_theme_warning_color.set(theme.warning);
    config_store().ui_theme_error_color.set(theme.error);
    config_store().ui_theme_image_color.set(theme.image);
    config_store().status_led_idle_color.set(theme.led_idle);
    config_store().status_led_printing_color.set(theme.led_printing);
    config_store().status_led_finished_color.set(theme.led_finished);
    config_store().status_led_warning_color.set(theme.led_warning);
    config_store().status_led_error_color.set(theme.led_error);
    reload_status_led_colors();
}

void save_imported_theme(const ThemeColors &theme) {
    config_store().usb_theme_primary_color.set(theme.primary);
    config_store().usb_theme_progress_color.set(theme.progress);
    config_store().usb_theme_warning_color.set(theme.warning);
    config_store().usb_theme_error_color.set(theme.error);
    config_store().usb_theme_image_color.set(theme.image);
    config_store().usb_status_led_idle_color.set(theme.led_idle);
    config_store().usb_status_led_printing_color.set(theme.led_printing);
    config_store().usb_status_led_finished_color.set(theme.led_finished);
    config_store().usb_status_led_warning_color.set(theme.led_warning);
    config_store().usb_status_led_error_color.set(theme.led_error);
}

ThemeColors imported_theme() {
    return {
        .primary = config_store().usb_theme_primary_color.get(),
        .progress = config_store().usb_theme_progress_color.get(),
        .warning = config_store().usb_theme_warning_color.get(),
        .error = config_store().usb_theme_error_color.get(),
        .image = config_store().usb_theme_image_color.get(),
        .led_idle = config_store().usb_status_led_idle_color.get(),
        .led_printing = config_store().usb_status_led_printing_color.get(),
        .led_finished = config_store().usb_status_led_finished_color.get(),
        .led_warning = config_store().usb_status_led_warning_color.get(),
        .led_error = config_store().usb_status_led_error_color.get(),
    };
}

bool parse_hex_color(const char *begin, uint32_t &out) {
    while (*begin == ' ' || *begin == '\t' || *begin == '"' || *begin == '#') {
        ++begin;
    }
    const char *end = begin;
    while ((*end >= '0' && *end <= '9') || (*end >= 'a' && *end <= 'f') || (*end >= 'A' && *end <= 'F')) {
        ++end;
    }
    uint32_t raw = 0;
    if (end == begin || std::from_chars(begin, end, raw, 16).ec != std::errc {} || raw > 0xffffff) {
        return false;
    }
    out = raw;
    return true;
}

bool find_json_color(const char *json, const char *key, uint32_t &out) {
    std::array<char, 40> quoted {};
    snprintf(quoted.data(), quoted.size(), "\"%s\"", key);
    const char *hit = strstr(json, quoted.data());
    if (!hit) {
        return false;
    }
    const char *colon = strchr(hit + strlen(quoted.data()), ':');
    return colon && parse_hex_color(colon + 1, out);
}

bool parse_theme_json(const char *json, ThemeColors &theme) {
    bool found = false;
    found |= find_json_color(json, "primary", theme.primary);
    found |= find_json_color(json, "progress", theme.progress);
    found |= find_json_color(json, "warning", theme.warning);
    found |= find_json_color(json, "error", theme.error);
    found |= find_json_color(json, "image", theme.image);
    found |= find_json_color(json, "ui_primary", theme.primary);
    found |= find_json_color(json, "ui_progress", theme.progress);
    found |= find_json_color(json, "ui_warning", theme.warning);
    found |= find_json_color(json, "ui_error", theme.error);
    found |= find_json_color(json, "ui_image", theme.image);
    found |= find_json_color(json, "led_idle", theme.led_idle);
    found |= find_json_color(json, "led_printing", theme.led_printing);
    found |= find_json_color(json, "led_finished", theme.led_finished);
    found |= find_json_color(json, "led_warning", theme.led_warning);
    found |= find_json_color(json, "led_error", theme.led_error);
    return found;
}
} // namespace

bool ui_theme_menu::load_usb_theme_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    std::array<char, 2048> json {};
    const size_t read = fread(json.data(), 1, json.size() - 1, file);
    fclose(file);
    if (read == 0) {
        return false;
    }

    ThemeColors theme = theme_default;
    if (!parse_theme_json(json.data(), theme)) {
        return false;
    }
    save_imported_theme(theme);
    apply_theme(theme);
    return true;
}

MI_UI_THEME_PRESET_PRUSA::MI_UI_THEME_PRESET_PRUSA()
    : IWindowMenuItem(_("Prusa"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRESET_PRUSA::click(IWindowMenu &) { apply_theme(theme_prusa); }

MI_UI_THEME_PRESET_DEFAULT::MI_UI_THEME_PRESET_DEFAULT()
    : IWindowMenuItem(_("Default"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRESET_DEFAULT::click(IWindowMenu &) { apply_theme(theme_default); }

MI_UI_THEME_PRESET_BLUE::MI_UI_THEME_PRESET_BLUE()
    : IWindowMenuItem(_("Blue"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRESET_BLUE::click(IWindowMenu &) { apply_theme(theme_blue); }

MI_UI_THEME_PRESET_GRAPHITE::MI_UI_THEME_PRESET_GRAPHITE()
    : IWindowMenuItem(_("Graphite"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRESET_GRAPHITE::click(IWindowMenu &) { apply_theme(theme_graphite); }

MI_UI_THEME_PRESET_FOREST::MI_UI_THEME_PRESET_FOREST()
    : IWindowMenuItem(_("Forest"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRESET_FOREST::click(IWindowMenu &) { apply_theme(theme_forest); }

MI_UI_THEME_CUSTOM_COLORS::MI_UI_THEME_CUSTOM_COLORS()
    : IWindowMenuItem(_("Custom"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_CUSTOM_COLORS::click(IWindowMenu &) { Screens::Access()->Open<ScreenMenuUiThemeCustom>(); }

MI_UI_THEME_IMPORTED_COLORS::MI_UI_THEME_IMPORTED_COLORS()
    : IWindowMenuItem(_("Imported"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_IMPORTED_COLORS::click(IWindowMenu &) { Screens::Access()->Open<ScreenMenuUiThemeImported>(); }

MI_UI_THEME_LOAD_USB::MI_UI_THEME_LOAD_USB()
    : IWindowMenuItem(_("Load from USB"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_LOAD_USB::click(IWindowMenu &) { Screens::Access()->Open<ScreenThemeFileBrowser>(); }

MI_UI_THEME_APPLY_IMPORTED::MI_UI_THEME_APPLY_IMPORTED()
    : IWindowMenuItem(_("Apply Imported"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_APPLY_IMPORTED::click(IWindowMenu &) { apply_theme(imported_theme()); }

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

MI_UI_THEME_IMAGE_COLOR::MI_UI_THEME_IMAGE_COLOR()
    : IWindowMenuItem(_("Image Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_IMAGE_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().ui_theme_image_color); }

ScreenMenuUiThemeColors::ScreenMenuUiThemeColors()
    : ScreenMenuUiThemeColors__(_("UI THEME")) {}

ScreenMenuUiThemeCustom::ScreenMenuUiThemeCustom()
    : ScreenMenuUiThemeCustom__(_("CUSTOM THEME")) {}

MI_USB_THEME_PRIMARY_COLOR::MI_USB_THEME_PRIMARY_COLOR()
    : IWindowMenuItem(_("Primary Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_USB_THEME_PRIMARY_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().usb_theme_primary_color); }

MI_USB_THEME_PROGRESS_COLOR::MI_USB_THEME_PROGRESS_COLOR()
    : IWindowMenuItem(_("Progress Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_USB_THEME_PROGRESS_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().usb_theme_progress_color); }

MI_USB_THEME_WARNING_COLOR::MI_USB_THEME_WARNING_COLOR()
    : IWindowMenuItem(_("Warning Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_USB_THEME_WARNING_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().usb_theme_warning_color); }

MI_USB_THEME_ERROR_COLOR::MI_USB_THEME_ERROR_COLOR()
    : IWindowMenuItem(_("Error Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_USB_THEME_ERROR_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().usb_theme_error_color); }

MI_USB_THEME_IMAGE_COLOR::MI_USB_THEME_IMAGE_COLOR()
    : IWindowMenuItem(_("Image Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_USB_THEME_IMAGE_COLOR::click(IWindowMenu &) { edit_color(GetLabel(), config_store().usb_theme_image_color); }

ScreenMenuUiThemeImported::ScreenMenuUiThemeImported()
    : ScreenMenuUiThemeImported__(_("IMPORTED THEME")) {}

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

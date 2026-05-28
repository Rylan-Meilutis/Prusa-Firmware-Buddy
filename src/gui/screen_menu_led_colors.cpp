#include "screen_menu_led_colors.hpp"

#include "ScreenHandler.hpp"
#include "screen_theme_filebrowser.hpp"
#include <config_store/store_instance.hpp>
#include <dialogs/dialog_text_input.hpp>
#include <option/has_leds.h>
#include <utils/string_builder.hpp>
#include <window_msgbox.hpp>
#define JSMN_HEADER
#include <jsmn.h>
#include <array>
#include <charconv>
#include <cctype>
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

constexpr std::array<const char *, 7> theme_preset_names {
    N_("Prusa"),
    N_("Indigo"),
    N_("Blue"),
    N_("Graphite"),
    N_("Forest"),
    N_("Imported"),
    N_("Custom"),
};

constexpr std::array<const ThemeColors *, 5> built_in_themes {
    &theme_prusa,
    &theme_default,
    &theme_blue,
    &theme_graphite,
    &theme_forest,
};

bool same_theme(const ThemeColors &lhs, const ThemeColors &rhs) {
    return lhs.primary == rhs.primary
        && lhs.progress == rhs.progress
        && lhs.warning == rhs.warning
        && lhs.error == rhs.error
        && lhs.image == rhs.image
        && lhs.led_idle == rhs.led_idle
        && lhs.led_printing == rhs.led_printing
        && lhs.led_finished == rhs.led_finished
        && lhs.led_warning == rhs.led_warning
        && lhs.led_error == rhs.led_error;
}

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

#if HAS_LEDS()
void reload_status_led_colors() {
    leds::StatusLedsHandler::instance().reload_colors();
}
#endif

void apply_theme(const ThemeColors &theme) {
    config_store().ui_theme_primary_color.set(theme.primary);
    config_store().ui_theme_progress_color.set(theme.progress);
    config_store().ui_theme_warning_color.set(theme.warning);
    config_store().ui_theme_error_color.set(theme.error);
    config_store().ui_theme_image_color.set(theme.image);
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

ThemeColors current_theme() {
    return {
        .primary = config_store().ui_theme_primary_color.get(),
        .progress = config_store().ui_theme_progress_color.get(),
        .warning = config_store().ui_theme_warning_color.get(),
        .error = config_store().ui_theme_error_color.get(),
        .image = config_store().ui_theme_image_color.get(),
        .led_idle = config_store().status_led_idle_color.get(),
        .led_printing = config_store().status_led_printing_color.get(),
        .led_finished = config_store().status_led_finished_color.get(),
        .led_warning = config_store().status_led_warning_color.get(),
        .led_error = config_store().status_led_error_color.get(),
    };
}

int current_theme_preset_index() {
    const ThemeColors current = current_theme();
    for (size_t i = 0; i < built_in_themes.size(); ++i) {
        if (same_theme(current, *built_in_themes[i])) {
            return static_cast<int>(i);
        }
    }
    if (same_theme(current, imported_theme())) {
        return static_cast<int>(built_in_themes.size());
    }
    return static_cast<int>(built_in_themes.size() + 1);
}

bool parse_hex_color_token(const char *json, const jsmntok_t &token, uint32_t &out) {
    if (token.type != JSMN_STRING || token.start < 0 || token.end < token.start) {
        return false;
    }

    const char *begin = json + token.start;
    size_t len = static_cast<size_t>(token.end - token.start);
    if (len == 7 && *begin == '#') {
        ++begin;
        --len;
    }
    if (len != 6) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(begin[i]))) {
            return false;
        }
    }

    uint32_t raw = 0;
    const char *end = begin + len;
    const auto result = std::from_chars(begin, end, raw, 16);
    if (result.ec != std::errc {} || result.ptr != end || raw > 0xffffff) {
        return false;
    }
    out = raw;
    return true;
}

bool token_equals(const char *json, const jsmntok_t &token, const char *text) {
    if (token.type != JSMN_STRING || token.start < 0 || token.end < token.start) {
        return false;
    }
    const size_t token_len = static_cast<size_t>(token.end - token.start);
    const size_t text_len = strlen(text);
    return token_len == text_len && strncmp(json + token.start, text, text_len) == 0;
}

bool token_is_unsigned_number(const char *json, const jsmntok_t &token) {
    if (token.type != JSMN_PRIMITIVE || token.start < 0 || token.end <= token.start) {
        return false;
    }
    for (int i = token.start; i < token.end; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(json[i]))) {
            return false;
        }
    }
    return true;
}

const jsmntok_t *skip_token(const jsmntok_t *token, const jsmntok_t *end) {
    if (token >= end) {
        return nullptr;
    }

    const jsmntok_t *pos = token + 1;
    if (token->type == JSMN_OBJECT) {
        for (int i = 0; i < token->size; ++i) {
            pos = skip_token(pos, end);
            if (!pos) {
                return nullptr;
            }
            pos = skip_token(pos, end);
            if (!pos) {
                return nullptr;
            }
        }
    } else if (token->type == JSMN_ARRAY) {
        for (int i = 0; i < token->size; ++i) {
            pos = skip_token(pos, end);
            if (!pos) {
                return nullptr;
            }
        }
    }
    return pos <= end ? pos : nullptr;
}

struct ColorField {
    const char *key;
    uint32_t ThemeColors::*value;
};

constexpr std::array<ColorField, 5> ui_theme_fields {
    ColorField { "primary", &ThemeColors::primary },
    ColorField { "progress", &ThemeColors::progress },
    ColorField { "warning", &ThemeColors::warning },
    ColorField { "error", &ThemeColors::error },
    ColorField { "image", &ThemeColors::image },
};

constexpr std::array<ColorField, 5> led_theme_fields {
    ColorField { "idle", &ThemeColors::led_idle },
    ColorField { "printing", &ThemeColors::led_printing },
    ColorField { "finished", &ThemeColors::led_finished },
    ColorField { "warning", &ThemeColors::led_warning },
    ColorField { "error", &ThemeColors::led_error },
};

template <size_t N>
bool parse_color_object(const char *json, const jsmntok_t *object, const jsmntok_t *tokens_end, const std::array<ColorField, N> &fields, ThemeColors &theme) {
    if (object >= tokens_end || object->type != JSMN_OBJECT || N >= 32) {
        return false;
    }

    uint32_t seen = 0;
    const jsmntok_t *pos = object + 1;
    for (int i = 0; i < object->size; ++i) {
        if (pos >= tokens_end || pos->type != JSMN_STRING) {
            return false;
        }
        const jsmntok_t *value = pos + 1;
        if (value >= tokens_end) {
            return false;
        }

        bool matched = false;
        for (size_t field_index = 0; field_index < N; ++field_index) {
            if (!token_equals(json, *pos, fields[field_index].key)) {
                continue;
            }

            const uint32_t bit = 1u << field_index;
            if ((seen & bit) != 0) {
                return false;
            }
            if (!parse_hex_color_token(json, *value, theme.*fields[field_index].value)) {
                return false;
            }
            seen |= bit;
            matched = true;
            break;
        }
        if (!matched) {
            return false;
        }

        pos = skip_token(value, tokens_end);
        if (!pos) {
            return false;
        }
    }

    return seen == ((1u << N) - 1u);
}

bool parse_theme_json(const char *json, size_t len, ThemeColors &theme) {
    std::array<jsmntok_t, 64> tokens {};
    jsmn_parser parser;
    jsmn_init(&parser);
    const int parsed = jsmn_parse(&parser, json, len, tokens.data(), tokens.size());
    if (parsed <= 0) {
        return false;
    }

    const jsmntok_t *tokens_begin = tokens.data();
    const jsmntok_t *tokens_end = tokens_begin + parsed;
    const jsmntok_t *root = tokens_begin;
    if (root->type != JSMN_OBJECT) {
        return false;
    }
    for (size_t i = static_cast<size_t>(root->end); i < len; ++i) {
        if (!std::isspace(static_cast<unsigned char>(json[i]))) {
            return false;
        }
    }

    bool has_ui = false;
    bool has_led = false;
    bool has_name = false;
    bool has_schema_version = false;
    const jsmntok_t *pos = root + 1;
    for (int i = 0; i < root->size; ++i) {
        if (pos >= tokens_end || pos->type != JSMN_STRING) {
            return false;
        }
        const jsmntok_t *value = pos + 1;
        if (value >= tokens_end) {
            return false;
        }

        if (token_equals(json, *pos, "name")) {
            if (has_name || value->type != JSMN_STRING) {
                return false;
            }
            has_name = true;
        } else if (token_equals(json, *pos, "schema_version")) {
            if (has_schema_version || !token_is_unsigned_number(json, *value)) {
                return false;
            }
            has_schema_version = true;
        } else if (token_equals(json, *pos, "ui")) {
            if (has_ui || !parse_color_object(json, value, tokens_end, ui_theme_fields, theme)) {
                return false;
            }
            has_ui = true;
        } else if (token_equals(json, *pos, "led")) {
            if (has_led || !parse_color_object(json, value, tokens_end, led_theme_fields, theme)) {
                return false;
            }
            has_led = true;
        } else {
            return false;
        }

        pos = skip_token(value, tokens_end);
        if (!pos) {
            return false;
        }
    }

    return has_ui && has_led;
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
    if (!parse_theme_json(json.data(), read, theme)) {
        return false;
    }
    save_imported_theme(theme);
    apply_theme(theme);
    return true;
}

MI_UI_THEME_PRESET_SELECT::MI_UI_THEME_PRESET_SELECT()
    : MenuItemSelectMenu(_("Theme")) {
    set_current_item(current_theme_preset_index());
}

int MI_UI_THEME_PRESET_SELECT::item_count() const {
    return theme_preset_names.size();
}

void MI_UI_THEME_PRESET_SELECT::build_item_text(int index, const std::span<char> &buffer) const {
    if (index < 0 || index >= item_count()) {
        return;
    }
    _(theme_preset_names[index]).copyToRAM(buffer);
}

bool MI_UI_THEME_PRESET_SELECT::on_item_selected([[maybe_unused]] int old_index, int new_index) {
    if (new_index < 0 || new_index >= item_count()) {
        return false;
    }
    if (new_index < static_cast<int>(built_in_themes.size())) {
        apply_theme(*built_in_themes[new_index]);
    } else if (new_index == static_cast<int>(built_in_themes.size())) {
        apply_theme(imported_theme());
    } else {
        Screens::Access()->Open<ScreenMenuUiThemeCustom>();
    }
    return true;
}

MI_UI_THEME_PRESET_PRUSA::MI_UI_THEME_PRESET_PRUSA()
    : IWindowMenuItem(_("Prusa"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_PRESET_PRUSA::click(IWindowMenu &) { apply_theme(theme_prusa); }

MI_UI_THEME_PRESET_DEFAULT::MI_UI_THEME_PRESET_DEFAULT()
    : IWindowMenuItem(_("Indigo"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
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

MI_UI_THEME_JSON_HELP::MI_UI_THEME_JSON_HELP()
    : IWindowMenuItem(_("Theme JSON Help"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}
void MI_UI_THEME_JSON_HELP::click(IWindowMenu &) {
    MsgBoxInfo(_("Theme JSON needs ui and led color objects. Use #RRGGBB strings. Required ui: primary, progress, warning, error, image. Required led: idle, printing, finished, warning, error."), Responses_Ok);
}

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

#if HAS_LEDS()
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
#endif

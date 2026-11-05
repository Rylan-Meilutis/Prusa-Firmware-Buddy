#include "screen_theme_filebrowser.hpp"

#include "ScreenHandler.hpp"
#include "gui_media_events.hpp"
#include "screen_menu_led_colors.hpp"
#include <guiconfig/GuiDefaults.hpp>
#include <img_resources.hpp>
#include <window_msgbox.hpp>
#include <array>
#include <buddy/filename_defs.hpp>
#include <cstring>

ScreenThemeFileBrowser::ScreenThemeFileBrowser()
    : screen_t()
    , header(this)
    , file_browser(this, GuiDefaults::RectScreenNoHeader, "/usb/theme.json") {
    header.SetIcon(&img::folder_full_16x16);
    header.SetText(_("THEMES"));
    CaptureNormalWindow(file_browser);
}

void ScreenThemeFileBrowser::windowEvent([[maybe_unused]] window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::MEDIA:
        check_missing_media(MediaState_t(reinterpret_cast<int>(param)));
        break;
    case GUI_event_t::LOOP:
        check_missing_media(GuiMediaEventsHandler::Get());
        break;
    case GUI_event_t::CHILD_CLICK: {
        WindowFileBrowser::event_conversion_union un;
        un.pvoid = param;
        switch (un.action) {
        case WindowFileBrowser::event_conversion_union::Action::go_home:
            go_home();
            return;
        case WindowFileBrowser::event_conversion_union::Action::file_selected:
            load_selected_file();
            return;
        }
    } break;
    default:
        break;
    }
}

void ScreenThemeFileBrowser::check_missing_media(MediaState_t media_state) {
    if (media_state == MediaState_t::removed) {
        browser().clear_first_visible_canonical();
        Screens::Access()->Get()->Validate();
        Screens::Access()->close_screen_or_dialog();
    }
}

void ScreenThemeFileBrowser::load_selected_file() {
    browser().save_top_canonical();

    std::array<char, filename_defs::path_buffer_size> path {};
    const auto written = browser().WriteNameToPrint(path.data(), path.size());
    if (written < 0 || static_cast<size_t>(written) >= path.size()) {
        MsgBoxError(_("Failed to load theme file."), Responses_Ok);
        return;
    }

    const char *ext = strrchr(path.data(), '.');
    if (!ext || strcasecmp(ext, ".json") != 0) {
        MsgBoxWarning(_("Select a JSON theme file."), Responses_Ok);
        return;
    }

    if (ui_theme_menu::load_usb_theme_file(path.data())) {
        MsgBoxInfo(_("Theme loaded."), Responses_Ok);
        Screens::Access()->close_screen_or_dialog();
    } else {
        MsgBoxError(_("Invalid theme JSON."), Responses_Ok);
    }
}

void ScreenThemeFileBrowser::go_home() {
    browser().clear_first_visible_canonical();
    Screens::Access()->close_screen_or_dialog();
}

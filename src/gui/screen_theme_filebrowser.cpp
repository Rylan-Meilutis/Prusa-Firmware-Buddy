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

const FileBrowserConfiguration ScreenThemeFileBrowser::configuration_ {
    .path = "/usb/theme.json",
    .title = N_("THEMES"),
    .filter = FileSort::FileFilter::PRINTABLE,
    .on_file_selected = &ScreenThemeFileBrowser::load_selected_file,
    .on_media_removed = &ScreenThemeFileBrowser::on_media_removed,
    .close = &ScreenThemeFileBrowser::close_browser,
};

ScreenThemeFileBrowser::ScreenThemeFileBrowser()
    : FileBrowserScreen(configuration_) {}

void ScreenThemeFileBrowser::on_media_removed() {
    Screens::Access()->Get()->Validate();
}

void ScreenThemeFileBrowser::close_browser() {
    Screens::Access()->close_screen_or_dialog();
}

void ScreenThemeFileBrowser::load_selected_file(const char *path) {
    if (!path) {
        MsgBoxError(_("Failed to load theme file."), Responses_Ok);
        return;
    }

    const char *ext = strrchr(path, '.');
    if (!ext || strcasecmp(ext, ".json") != 0) {
        MsgBoxWarning(_("Select a JSON theme file."), Responses_Ok);
        return;
    }

    if (ui_theme_menu::load_usb_theme_file(path)) {
        MsgBoxInfo(_("Theme loaded."), Responses_Ok);
        Screens::Access()->close_screen_or_dialog();
    } else {
        MsgBoxError(_("Invalid theme JSON."), Responses_Ok);
    }
}

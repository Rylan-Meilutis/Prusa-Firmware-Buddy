#pragma once

#include "screen.hpp"
#include "window_filebrowser.hpp"
#include "window_header.hpp"

class ScreenThemeFileBrowser final : public screen_t {
    window_header_t header;
    WindowExtendedMenu<WindowFileBrowser> file_browser;

    WindowFileBrowser &browser() {
        return file_browser.menu;
    }

    void load_selected_file();
    void go_home();
    void check_missing_media(MediaState_t media_state);

public:
    ScreenThemeFileBrowser();

protected:
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};

#pragma once

#include "screen_menu.hpp"
#include "window_filebrowser.hpp"
#include <window_header.hpp>

class MI_FIRMWARE_SELECT_USB final : public IWindowMenuItem {
public:
    MI_FIRMWARE_SELECT_USB();

protected:
    void click(IWindowMenu &) override;
};

#if !HAS_MINI_DISPLAY()
class MI_FIRMWARE_UPDATE_HELP final : public IWindowMenuItem {
public:
    MI_FIRMWARE_UPDATE_HELP();

protected:
    void click(IWindowMenu &) override;
};
#endif

using ScreenMenuFirmwareUpdate_ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    MI_FIRMWARE_SELECT_USB
#if !HAS_MINI_DISPLAY()
    , MI_FIRMWARE_UPDATE_HELP
#endif
    >;

class ScreenMenuFirmwareUpdate final : public ScreenMenuFirmwareUpdate_ {
public:
    ScreenMenuFirmwareUpdate();
};

class ScreenFirmwareFileBrowser final : public screen_t {
    window_header_t header;
    WindowExtendedMenu<WindowFileBrowser> file_browser;

    WindowFileBrowser &browser() { return file_browser.menu; }
    void check_missing_media(MediaState_t media_state);
    void select_file();
    void go_home();

public:
    ScreenFirmwareFileBrowser();
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};

#pragma once

#include "screen_menu.hpp"
#include "MItem_menus.hpp"
#include <gui/screen/screen_menu_virtual.hpp>
#include "screen_filebrowser.hpp"
#include <window_header.hpp>

using MI_FIRMWARE_SELECT_USB = MI_SCREEN<N_("Select BBF from USB"), class ScreenFirmwareFileBrowser>;
using MI_FIRMWARE_UPDATE_HELP = MI_SCREEN<N_("Update Instructions"), class ScreenHelpFWUpdate>;

class ScreenFirmwareFileBrowser final : public FileBrowserScreen {
    static const FileBrowserConfiguration configuration_;
    static void select_file(const char *path);
    static void close_browser();

public:
    ScreenFirmwareFileBrowser();
};

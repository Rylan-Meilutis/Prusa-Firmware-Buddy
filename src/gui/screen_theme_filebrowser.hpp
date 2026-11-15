#pragma once

#include "screen_filebrowser.hpp"

class ScreenThemeFileBrowser final : public FileBrowserScreen {
    static const FileBrowserConfiguration configuration_;
    static void load_selected_file(const char *path);
    static void on_media_removed();
    static void close_browser();

public:
    ScreenThemeFileBrowser();
};

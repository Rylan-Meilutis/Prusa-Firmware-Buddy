#pragma once
#include "gui.hpp"
#include <window_filebrowser.hpp>
#include "window_header.hpp"
#include "screen.hpp"
#include "window_dlg_wait.hpp"

struct FileBrowserConfiguration {
    const char *path;
    const char *title;
    FileSort::FileFilter filter;
    bool save_top = true;
    void (*on_file_selected)(const char *);
    void (*on_media_removed)() = nullptr;
    void (*close)() = nullptr;
};

// One navigation implementation, with separate immutable selection policies.
class FileBrowserScreen : public screen_t {
    const FileBrowserConfiguration &configuration_;
    window_header_t header;
    WindowExtendedMenu<WindowFileBrowser> file_browser;
    WindowFileBrowser &browser() { return file_browser.menu; }
    void select_file();
    void goHome();
    void checkMissingMedia(MediaState_t media_state);
    void close_browser();

protected:
    FileBrowserScreen(const FileBrowserConfiguration &configuration, const char *path = nullptr);
    void windowEvent(window_t *sender, GUI_event_t event, void *param) final;
};

class screen_filebrowser_data_t : public FileBrowserScreen {
    static const FileBrowserConfiguration configuration_;
    static void printTheFile(const char *path);

public:
    screen_filebrowser_data_t();
};

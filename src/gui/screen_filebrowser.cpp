#include "screen_filebrowser.hpp"
#include <array>

#include "config.h"
#include "stdlib.h"
#include "usb_host.h"
#include "cmsis_os.h"
#include "marlin_client.hpp"
#include "print_utils.hpp"
#include "ScreenHandler.hpp"
#include "i18n.h"
#include "gui_media_events.hpp"
#include <logging/log.hpp>
#include "img_resources.hpp"
#include <guiconfig/GuiDefaults.hpp>
#include <buddy/filename_defs.hpp>

#include "../Marlin/src/gcode/queue.h"
#include "../Marlin/src/gcode/lcd/M73_PE.h"

LOG_COMPONENT_REF(GUI);

const FileBrowserConfiguration screen_filebrowser_data_t::configuration_ {
    .path = "/usb",
    .title = N_("PROJECTS"),
    .filter = FileSort::FileFilter::PRINTABLE,
    .on_file_selected = &screen_filebrowser_data_t::printTheFile,
};

FileBrowserScreen::FileBrowserScreen(const FileBrowserConfiguration &configuration, const char *path)
    : screen_t()
    , configuration_(configuration)
    , header(this)
    , file_browser(this, GuiDefaults::RectScreenNoHeader, path ? path : configuration.path, configuration.filter) {
    header.SetIcon(&img::folder_full_16x16);
    header.SetText(_(configuration.title));
    CaptureNormalWindow(file_browser);
}

screen_filebrowser_data_t::screen_filebrowser_data_t()
    : FileBrowserScreen(configuration_, GCodeInfo::getInstance().GetGcodeFilepath()) {}

void FileBrowserScreen::select_file() {
    if (configuration_.save_top) {
        browser().SaveTopSFN();
    }
    std::array<char, filename_defs::path_buffer_size> path {};
    const auto written = browser().WriteNameToPrint(path.data(), path.size());
    configuration_.on_file_selected(written < 0 || static_cast<size_t>(written) >= path.size() ? nullptr : path.data());
}

void FileBrowserScreen::windowEvent([[maybe_unused]] window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::MEDIA:
        checkMissingMedia(MediaState_t(reinterpret_cast<int>(param)));
        break;
    case GUI_event_t::LOOP: // If media is removed during preprint, the even won't come
        checkMissingMedia(GuiMediaEventsHandler::Get());
        break;
    case GUI_event_t::CHILD_CLICK: {
        WindowFileBrowser::event_conversion_union un;
        un.pvoid = param;
        switch (un.action) {
        case WindowFileBrowser::event_conversion_union::Action::go_home:
            goHome();
            return;
        case WindowFileBrowser::event_conversion_union::Action::file_selected:
            select_file();
            return;
        }
    } break;
    default:
        break;
    }
}

void FileBrowserScreen::checkMissingMedia(MediaState_t media_state) {
    if (media_state == MediaState_t::removed || media_state == MediaState_t::error) {
        browser().clear_first_visible_sfn();
        if (configuration_.on_media_removed) {
            configuration_.on_media_removed();
        }
        close_browser();
    }
}

void screen_filebrowser_data_t::printTheFile(const char *path) {
    if (!path) {
        log_error(GUI, "Failed to prepare file path for print");
        return;
    }
    marlin_client::print_start(path);
}

void FileBrowserScreen::goHome() {
    browser().clear_first_visible_sfn();
    close_browser();
}

void FileBrowserScreen::close_browser() {
    if (configuration_.close) {
        configuration_.close();
        return;
    }
    Screens::Access()->Close();
}

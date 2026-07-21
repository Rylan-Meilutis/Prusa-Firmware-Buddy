#include "screen_firmware_update.hpp"

#include "ScreenHandler.hpp"
#include "gui_media_events.hpp"
#include "screen_help_fw_update.hpp"
#include <common/sys.hpp>
#include <data_exchange.hpp>
#include <guiconfig/GuiDefaults.hpp>
#include <img_resources.hpp>
#include <window_msgbox.hpp>
#include <array>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {
constexpr const char *staged_firmware_path = "/usb/FWUPD.BBF";
constexpr const char *staged_firmware_marker_path = "/usb/FWUPD.UI";

bool stage_firmware_for_bootloader(const char *source_path) {
    const bool source_is_staging_path = strcasecmp(source_path, staged_firmware_path) == 0;

    if (!source_is_staging_path) {
        FILE *source = fopen(source_path, "rb");
        if (!source) return false;

        FILE *destination = fopen(staged_firmware_path, "wb");
        if (!destination) {
            fclose(source);
            return false;
        }

        std::array<uint8_t, 1024> buffer {};
        bool success = true;
        while (const size_t read = fread(buffer.data(), 1, buffer.size(), source)) {
            if (fwrite(buffer.data(), 1, read, destination) != read) {
                success = false;
                break;
            }
        }
        success = success && !ferror(source) && fflush(destination) == 0 && fsync(fileno(destination)) == 0;
        success = fclose(destination) == 0 && success;
        fclose(source);

        if (!success) {
            remove(staged_firmware_path);
            return false;
        }
    }

    FILE *marker = fopen(staged_firmware_marker_path, "wb");
    if (!marker) {
        if (!source_is_staging_path) remove(staged_firmware_path);
        return false;
    }
    bool success = fflush(marker) == 0 && fsync(fileno(marker)) == 0;
    success = fclose(marker) == 0 && success;
    if (!success) {
        remove(staged_firmware_marker_path);
        if (!source_is_staging_path) remove(staged_firmware_path);
    }
    return success;
}
} // namespace

MI_FIRMWARE_SELECT_USB::MI_FIRMWARE_SELECT_USB()
    : IWindowMenuItem(_("Select BBF from USB"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_FIRMWARE_SELECT_USB::click(IWindowMenu &) {
    Screens::Access()->Open<ScreenFirmwareFileBrowser>();
}

MI_FIRMWARE_UPDATE_HELP::MI_FIRMWARE_UPDATE_HELP()
    : IWindowMenuItem(_("Update Instructions"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_FIRMWARE_UPDATE_HELP::click(IWindowMenu &) {
    Screens::Access()->Open<ScreenHelpFWUpdate>();
}

ScreenMenuFirmwareUpdate::ScreenMenuFirmwareUpdate()
    : ScreenMenuFirmwareUpdate_(_("FIRMWARE UPDATE")) {}

ScreenFirmwareFileBrowser::ScreenFirmwareFileBrowser()
    : screen_t()
    , header(this)
    , file_browser(this, GuiDefaults::RectScreenNoHeader, "/usb/firmware.bbf", FileSort::FileFilter::FIRMWARE) {
    header.SetIcon(&img::folder_full_16x16);
    header.SetText(_("SELECT FIRMWARE"));
    CaptureNormalWindow(file_browser);
}

void ScreenFirmwareFileBrowser::windowEvent([[maybe_unused]] window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::MEDIA:
        check_missing_media(MediaState_t(reinterpret_cast<int>(param)));
        break;
    case GUI_event_t::LOOP:
        check_missing_media(GuiMediaEventsHandler::Get());
        break;
    case GUI_event_t::CHILD_CLICK: {
        WindowFileBrowser::event_conversion_union action;
        action.pvoid = param;
        if (action.action == WindowFileBrowser::event_conversion_union::Action::go_home) {
            go_home();
        } else if (action.action == WindowFileBrowser::event_conversion_union::Action::file_selected) {
            select_file();
        }
        return;
    }
    default:
        break;
    }
}

void ScreenFirmwareFileBrowser::check_missing_media(MediaState_t media_state) {
    if (media_state == MediaState_t::removed || media_state == MediaState_t::error) {
        browser().clear_first_visible_sfn();
        Screens::Access()->Close();
    }
}

void ScreenFirmwareFileBrowser::select_file() {
    std::array<char, FILE_PATH_BUFFER_LEN> path {};
    const int written = browser().WriteNameToPrint(path.data(), path.size());
    if (written < 0 || static_cast<size_t>(written) >= path.size()) {
        MsgBoxError(_("Firmware path is too long."), Responses_Ok);
        return;
    }

    static constexpr char usb_prefix[] = "/usb/";
    const char *filename = strrchr(path.data(), '/');
    filename = filename ? filename + 1 : path.data();
    const char *extension = strrchr(filename, '.');
    if (strncmp(path.data(), usb_prefix, 5) != 0
        || strstr(path.data(), "/../") != nullptr
        || !extension
        || strcasecmp(extension, ".bbf") != 0) {
        MsgBoxWarning(_("Select a completed .bbf file from the USB drive."), Responses_Ok);
        return;
    }

    if (MsgBoxQuestion(_("Install selected firmware and restart?"), Responses_YesNo) != Response::Yes) {
        return;
    }

    if (!stage_firmware_for_bootloader(path.data())) {
        MsgBoxError(_("Could not stage the selected firmware as /usb/FWUPD.BBF."), Responses_Ok);
        return;
    }

    data_exchange::set_reflash_bbf_sfn("FWUPD.BBF");
    HAL_Delay(10);
    sys_reset();
}

void ScreenFirmwareFileBrowser::go_home() {
    browser().clear_first_visible_sfn();
    Screens::Access()->Close();
}

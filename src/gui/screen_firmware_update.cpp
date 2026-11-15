#include "screen_firmware_update.hpp"

#include "ScreenHandler.hpp"
#include "gui_media_events.hpp"
#include "screen_help_fw_update.hpp"
#include <guiconfig/GuiDefaults.hpp>
#include <img_resources.hpp>
#include <marlin_client.hpp>
#include <window_msgbox.hpp>
#include <array>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <buddy/filename_defs.hpp>
#include <firmware_cleanup_receipt.hpp>
#include <transfers/monitor.hpp>

namespace {
// A neutral extension prevents bootloader auto-discovery. Installation is
// authorized only by the retained M997 request created after confirmation.
constexpr const char *staged_firmware_path = "/usb/FWUPD.RME";

bool stage_firmware_for_bootloader(const char *source_path) {
    auto slot = transfers::Monitor::instance.allocate(
        transfers::Monitor::Type::Link, staged_firmware_path, 0);
    if (!slot || !firmware_update_handoff::clear_cleanup_authorization()) {
        return false;
    }
    // This runs on the GUI task. Keep the copy buffer out of its constrained
    // stack; overflowing that stack made the picker appear to freeze before
    // it could request the reboot.
    static std::array<uint8_t, 1024> buffer;
    const bool source_is_staging_path = strcasecmp(source_path, staged_firmware_path) == 0;

    if (!source_is_staging_path) {
        // Same-sized replacement images must not reuse an older status hash.
        for (const char *metadata : { "/usb/FWUPD.RME.rme-verified", "/usb/FWUPD.RME.rme-verified-tmp" }) {
            errno = 0;
            if (remove(metadata) != 0 && errno != ENOENT) {
                return false;
            }
        }
        FILE *source = fopen(source_path, "rb");
        if (!source) {
            return false;
        }

        FILE *destination = fopen(staged_firmware_path, "wb");
        if (!destination) {
            fclose(source);
            return false;
        }

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

    slot->done(transfers::Monitor::Outcome::Finished);
    return true;
}
} // namespace

namespace {
constexpr screen_menu_virtual::Configuration firmware_update_menu {
    .item_count = []() -> int { return 3; },
    .item_constructor = [](WindowMenuVirtual::ItemVariant &item, int index) {
        switch (index) {
        case 0: item.emplace<MI_RETURN>(); break;
        case 1: item.emplace<MI_FIRMWARE_SELECT_USB>(); break;
        case 2: item.emplace<MI_FIRMWARE_UPDATE_HELP>(); break;
        } },
    .title = N_("FIRMWARE UPDATE"),
    .footer = GuiDefaults::MenuFooter,
};
}

MI_HELP_FW_UPDATE::MI_HELP_FW_UPDATE()
    : MI_SCREEN_BASE(ScreenFactory::ScreenWithArg<ScreenMenuVirtual>(&firmware_update_menu), N_("Firmware Update"), &img::question_16x16) {}

const FileBrowserConfiguration ScreenFirmwareFileBrowser::configuration_ {
    .path = "/usb/firmware.bbf",
    .title = N_("SELECT FIRMWARE"),
    .filter = FileSort::FileFilter::FIRMWARE,
    .save_top = false,
    .on_file_selected = &ScreenFirmwareFileBrowser::select_file,
    .close = &ScreenFirmwareFileBrowser::close_browser,
};

ScreenFirmwareFileBrowser::ScreenFirmwareFileBrowser()
    : FileBrowserScreen(configuration_) {}

void ScreenFirmwareFileBrowser::select_file(const char *path) {
    if (!path) {
        MsgBoxError(_("Firmware path is too long."), Responses_Ok);
        return;
    }

    static constexpr char usb_prefix[] = "/usb/";
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    const char *extension = strrchr(filename, '.');
    if (strncmp(path, usb_prefix, 5) != 0
        || strstr(path, "/../") != nullptr
        || !extension
        || strcasecmp(extension, ".bbf") != 0) {
        MsgBoxWarning(_("Select a completed .bbf file from the USB drive."), Responses_Ok);
        return;
    }

    if (MsgBoxQuestion(_("Install selected firmware and restart?"), Responses_YesNo) != Response::Yes) {
        return;
    }

    if (!stage_firmware_for_bootloader(path)) {
        MsgBoxError(_("Could not stage the selected firmware for installation."), Responses_Ok);
        return;
    }

    // Use the same Marlin-side handoff as serial firmware updates. It sends
    // the command acknowledgement, writes the retained bootloader request,
    // and resets outside the GUI callback.
    marlin_client::gcode("M997 /usb/FWUPD.RME");
}

void ScreenFirmwareFileBrowser::close_browser() {
    Screens::Access()->Close();
}

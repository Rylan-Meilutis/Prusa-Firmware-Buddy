/**
 * @file screen_menu_settings.cpp
 */

#include "screen_menu_settings.hpp"
#include "screen_menu_experimental_settings.hpp"
#include "screen_help_fw_update.hpp"
#include "ScreenHandler.hpp"
#include "rme_settings_gcode.hpp"
#include "netdev.h"
#include "wui.h"

#include "knob_event.hpp"
#include "img_resources.hpp"
#include <guiconfig/GuiDefaults.hpp>
#include <window_msgbox.hpp>

#include <cstdio>

using namespace buddy;

/*****************************************************************************/
// MI_HELP_FW_UPDATE
MI_HELP_FW_UPDATE::MI_HELP_FW_UPDATE()
    : IWindowMenuItem(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
}

void MI_HELP_FW_UPDATE::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenHelpFWUpdate>);
}

/*****************************************************************************/
// MI_EXPORT_RME_SETTINGS
MI_EXPORT_RME_SETTINGS::MI_EXPORT_RME_SETTINGS()
    : IWindowMenuItem(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
}

void MI_EXPORT_RME_SETTINGS::click(IWindowMenu & /*window_menu*/) {
    static constexpr const char *path = "/usb/rme_settings.gcode";
    FILE *file = fopen(path, "w");
    if (!file) {
        MsgBoxError(_("Insert USB drive."), Responses_Ok);
        return;
    }

    const bool ok = rme_settings_gcode::write(file);
    const bool close_ok = fclose(file) == 0;
    if (ok && close_ok) {
        MsgBoxInfo(_("Exported to USB."), Responses_Ok);
    } else {
        MsgBoxError(_("Export failed."), Responses_Ok);
    }
}

/*****************************************************************************/
// MI_PID_SETTINGS
MI_PID_SETTINGS::MI_PID_SETTINGS()
    : IWindowMenuItem(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
}

void MI_PID_SETTINGS::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuPid>);
}

ScreenMenuSettings::ScreenMenuSettings()
    : ScreenMenuSettings__(_("SETTINGS"))
    , old_action(gui::knob::GetLongPressScreenAction()) { // backup hold action

    EnableLongHoldScreenAction();

#if (!PRINTER_IS_PRUSA_MINI())
    header.SetIcon(&img::settings_16x16);
#endif // PRINTER_IS_PRUSA_MINI()

    gui::knob::RegisterLongPressScreenAction([]() { Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuExperimentalSettings>); }); // new hold action
    EnableLongHoldScreenAction();
}

ScreenMenuSettings::~ScreenMenuSettings() {
    gui::knob::RegisterLongPressScreenAction(old_action); // restore hold action
}

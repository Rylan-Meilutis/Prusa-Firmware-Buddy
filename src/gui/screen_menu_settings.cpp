/// @file
#include "screen_menu_settings.hpp"

#include "screen_menu_experimental_settings.hpp"
#include "ScreenHandler.hpp"
#include "knob_event.hpp"
#include "img_resources.hpp"
#include <rme_settings_gcode.hpp>
#include <window_msgbox.hpp>
#include <cstdio>

MI_EXPORT_RME_SETTINGS::MI_EXPORT_RME_SETTINGS()
    : IWindowMenuItem(_("Export RME Settings"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_EXPORT_RME_SETTINGS::click(IWindowMenu &) {
    FILE *file = fopen("/usb/rme_settings.gcode", "w");
    if (!file) {
        MsgBoxError(_("Insert USB drive."), Responses_Ok);
        return;
    }
    setvbuf(file, nullptr, _IONBF, 0);
    const bool ok = rme_settings_gcode::write(file);
    const bool closed = fclose(file) == 0;
    if (ok && closed) {
        MsgBoxInfo(_("Exported to USB."), Responses_Ok);
    } else {
        MsgBoxError(_("Export failed."), Responses_Ok);
    }
}

ScreenMenuSettings::ScreenMenuSettings()
    : ScreenMenuSettingsBase {
        _("SETTINGS"),
        &img::settings_16x16,
    }
    , old_action(gui::knob::GetLongPressScreenAction()) { // backup hold action
    gui::knob::RegisterLongPressScreenAction([]() { Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuExperimentalSettings>); }); // new hold action
    EnableLongHoldScreenAction();
}

ScreenMenuSettings::~ScreenMenuSettings() {
    gui::knob::RegisterLongPressScreenAction(old_action); // restore hold action
}

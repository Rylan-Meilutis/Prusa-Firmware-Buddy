#include "screen_menu_hardware.hpp"
#include "screen_menu_experimental_settings.hpp"
#include "ScreenHandler.hpp"

ScreenMenuHardware::ScreenMenuHardware()
    : ScreenMenuHardware__(_(label)) {
}

void ScreenMenuHardware::windowEvent(window_t *sender, GUI_event_t event, void *param) {

    switch (event) {
    case GUI_event_t::HELD_RELEASED:
        Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuExperimentalSettings>);
        return;
    default:
        ScreenMenu::windowEvent(sender, event, param);
    }
}

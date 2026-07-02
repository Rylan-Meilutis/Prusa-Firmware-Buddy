/// @file
#include "screen_menu_leds.hpp"

ScreenMenuLeds::ScreenMenuLeds()
    : ScreenMenuLeds__ {
        _("LIGHTS SETTINGS"),
    } {
    EnableLongHoldScreenAction();
}

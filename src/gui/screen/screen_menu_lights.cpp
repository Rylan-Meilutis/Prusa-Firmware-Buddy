/// @file
#include "screen_menu_lights.hpp"

ScreenMenuLights::ScreenMenuLights()
    : ScreenMenuLightsBase {
        _("LIGHTS"),
    } {
    EnableLongHoldScreenAction();
}

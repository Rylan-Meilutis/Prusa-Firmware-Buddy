/**
 * @file screen_menu_leds.cpp
 */

#include "screen_menu_leds.hpp"

ScreenMenuLeds::ScreenMenuLeds()
    : ScreenMenuLeds__(_("LIGHTS SETTINGS")) {
    EnableLongHoldScreenAction();
}

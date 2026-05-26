/**
 * @file screen_menu_led_state.cpp
 */

#include "screen_menu_led_state.hpp"

#if HAS_SIDE_LEDS()

ScreenMenuLedDeepIdle::ScreenMenuLedDeepIdle()
    : ScreenMenuLedDeepIdle__(_("DEEP IDLE LIGHTS")) {
}

ScreenMenuLedIdle::ScreenMenuLedIdle()
    : ScreenMenuLedIdle__(_("IDLE LIGHTS")) {
}

ScreenMenuLedActive::ScreenMenuLedActive()
    : ScreenMenuLedActive__(_("ACTIVE LIGHTS")) {
}

ScreenMenuLedPrinting::ScreenMenuLedPrinting()
    : ScreenMenuLedPrinting__(_("PRINTING LIGHTS")) {
}

#endif

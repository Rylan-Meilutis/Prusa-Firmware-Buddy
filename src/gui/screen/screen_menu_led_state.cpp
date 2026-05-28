/**
 * @file screen_menu_led_state.cpp
 */

#include "screen_menu_led_state.hpp"

#if HAS_SIDE_LEDS() || HAS_LEDS() || HAS_SCREEN_BRIGHTNESS_SETTINGS() || (HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY())

ScreenMenuLedStateContainer::ScreenMenuLedStateContainer(leds::LightState state)
#if HAS_SIDE_LEDS()
    : item_main(state)
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    #if HAS_SIDE_LEDS()
    , item_external(state)
    #else
    : item_external(state)
    #endif
#endif
#if HAS_SIDE_LEDS()
    , item_brightness(state)
#endif
#if HAS_LEDS()
    #if HAS_SIDE_LEDS() || (HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY())
    , item_status_brightness(state)
    #else
    : item_status_brightness(state)
    #endif
#endif
#if HAS_SCREEN_BRIGHTNESS_SETTINGS()
    #if HAS_SIDE_LEDS() || HAS_LEDS() || (HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY())
    , item_screen_brightness(state)
    #else
    : item_screen_brightness(state)
    #endif
#endif
{
#if HAS_SIDE_LEDS() && HAS_DOOR_SENSOR()
    item_door_active.set_is_hidden(state != leds::LightState::active);
#else
    (void)state;
#endif
}

int ScreenMenuLedStateContainer::GetRawCount() const {
    int count = 2; // Return + hidden endstop
#if HAS_SIDE_LEDS()
    count += 2; // state enable + side/chamber brightness
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    ++count;
#endif
#if HAS_LEDS()
    ++count;
#endif
#if HAS_SCREEN_BRIGHTNESS_SETTINGS()
    ++count;
#endif
#if HAS_SIDE_LEDS() && HAS_DOOR_SENSOR()
    ++count;
#endif
    return count;
}

IWindowMenuItem *ScreenMenuLedStateContainer::GetItemByRawIndex(int pos) const {
    int index = 0;
    if (pos == index++) {
        return &item_return;
    }
#if HAS_SIDE_LEDS()
    if (pos == index++) {
        return &item_main;
    }
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    if (pos == index++) {
        return &item_external;
    }
#endif
#if HAS_SIDE_LEDS()
    if (pos == index++) {
        return &item_brightness;
    }
#endif
#if HAS_LEDS()
    if (pos == index++) {
        return &item_status_brightness;
    }
#endif
#if HAS_SCREEN_BRIGHTNESS_SETTINGS()
    if (pos == index++) {
        return &item_screen_brightness;
    }
#endif
#if HAS_SIDE_LEDS() && HAS_DOOR_SENSOR()
    if (pos == index++) {
        return &item_door_active;
    }
#endif
    return pos == index ? &item_hidden : nullptr;
}

int ScreenMenuLedStateContainer::GetRawIndex(IWindowMenuItem &item) const {
    for (int pos = 0; pos < GetRawCount(); ++pos) {
        if (GetItemByRawIndex(pos) == &item) {
            return pos;
        }
    }
    return GetRawCount();
}

ScreenMenuLedState::ScreenMenuLedState(const string_view_utf8 &label, leds::LightState state)
    : ScreenMenuBase(nullptr, label, GuiDefaults::MenuFooter)
    , container(state) {
    menu.menu.BindContainer(container);
}

ScreenMenuLedDeepIdle::ScreenMenuLedDeepIdle()
    : ScreenMenuLedState(_("DEEP IDLE LIGHTS"), leds::LightState::deep_idle) {
}

ScreenMenuLedIdle::ScreenMenuLedIdle()
    : ScreenMenuLedState(_("IDLE LIGHTS"), leds::LightState::idle) {
}

ScreenMenuLedActive::ScreenMenuLedActive()
    : ScreenMenuLedState(_("ACTIVE LIGHTS"), leds::LightState::active) {
}

ScreenMenuLedPrinting::ScreenMenuLedPrinting()
    : ScreenMenuLedState(_("PRINTING LIGHTS"), leds::LightState::printing) {
}

#endif

/**
 * @file screen_menu_led_state.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"
#include <guiconfig/guiconfig.h>
#include <option/has_door_sensor.h>

#ifndef HAS_SCREEN_BRIGHTNESS_SETTINGS
    #define HAS_SCREEN_BRIGHTNESS_SETTINGS() (HAS_ILI9488_DISPLAY() || HAS_ST7789_DISPLAY())
#endif

#if HAS_SIDE_LEDS() || HAS_LEDS() || HAS_SCREEN_BRIGHTNESS_SETTINGS() || (HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY())

class ScreenMenuLedStateContainer : public IWinMenuContainer {
public:
    ScreenMenuLedStateContainer(leds::LightState state);

    virtual int GetRawCount() const override;
    virtual IWindowMenuItem *GetItemByRawIndex(int pos) const override;
    virtual int GetRawIndex(IWindowMenuItem &item) const override;

private:
    mutable MI_RETURN item_return;
#if HAS_SIDE_LEDS()
    mutable MI_LIGHT_STATE_MAIN_ENABLE item_main;
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    mutable MI_LIGHT_STATE_EXTERNAL_ENABLE item_external;
#endif
#if HAS_SIDE_LEDS()
    mutable MI_LIGHT_STATE_BRIGHTNESS item_brightness;
#endif
#if HAS_LEDS()
    mutable MI_LIGHT_STATE_STATUS_BRIGHTNESS item_status_brightness;
#endif
#if HAS_SCREEN_BRIGHTNESS_SETTINGS()
    mutable MI_LIGHT_STATE_SCREEN_BRIGHTNESS item_screen_brightness;
#endif
#if HAS_SIDE_LEDS() && HAS_DOOR_SENSOR()
    mutable MI_LIGHT_STATE_DOOR_ACTIVE item_door_active;
#endif
    mutable MI_ALWAYS_HIDDEN item_hidden;
};

class ScreenMenuLedState : public ScreenMenuBase<WindowMenu> {
protected:
    ScreenMenuLedState(const string_view_utf8 &label, leds::LightState state);

private:
    ScreenMenuLedStateContainer container;
};

class ScreenMenuLedDeepIdle : public ScreenMenuLedState {
public:
    ScreenMenuLedDeepIdle();
};

class ScreenMenuLedIdle : public ScreenMenuLedState {
public:
    ScreenMenuLedIdle();
};

class ScreenMenuLedActive : public ScreenMenuLedState {
public:
    ScreenMenuLedActive();
};

class ScreenMenuLedPrinting : public ScreenMenuLedState {
public:
    ScreenMenuLedPrinting();
};

#endif

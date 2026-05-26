/**
 * @file screen_menu_led_state.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"

#if HAS_SIDE_LEDS()

using ScreenMenuLedDeepIdle__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    WithConstructorArgs<MI_LIGHT_STATE_MAIN_ENABLE, leds::LightState::deep_idle>,
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    WithConstructorArgs<MI_LIGHT_STATE_EXTERNAL_ENABLE, leds::LightState::deep_idle>,
#endif
    WithConstructorArgs<MI_LIGHT_STATE_BRIGHTNESS, leds::LightState::deep_idle>,
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLedDeepIdle : public ScreenMenuLedDeepIdle__ {
public:
    ScreenMenuLedDeepIdle();
};

using ScreenMenuLedIdle__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    WithConstructorArgs<MI_LIGHT_STATE_MAIN_ENABLE, leds::LightState::idle>,
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    WithConstructorArgs<MI_LIGHT_STATE_EXTERNAL_ENABLE, leds::LightState::idle>,
#endif
    WithConstructorArgs<MI_LIGHT_STATE_BRIGHTNESS, leds::LightState::idle>,
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLedIdle : public ScreenMenuLedIdle__ {
public:
    ScreenMenuLedIdle();
};

using ScreenMenuLedActive__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    WithConstructorArgs<MI_LIGHT_STATE_MAIN_ENABLE, leds::LightState::active>,
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    WithConstructorArgs<MI_LIGHT_STATE_EXTERNAL_ENABLE, leds::LightState::active>,
#endif
    WithConstructorArgs<MI_LIGHT_STATE_BRIGHTNESS, leds::LightState::active>,
    MI_LIGHT_STATE_DOOR_ACTIVE,
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLedActive : public ScreenMenuLedActive__ {
public:
    ScreenMenuLedActive();
};

using ScreenMenuLedPrinting__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    WithConstructorArgs<MI_LIGHT_STATE_MAIN_ENABLE, leds::LightState::printing>,
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    WithConstructorArgs<MI_LIGHT_STATE_EXTERNAL_ENABLE, leds::LightState::printing>,
#endif
    WithConstructorArgs<MI_LIGHT_STATE_BRIGHTNESS, leds::LightState::printing>,
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLedPrinting : public ScreenMenuLedPrinting__ {
public:
    ScreenMenuLedPrinting();
};

#endif

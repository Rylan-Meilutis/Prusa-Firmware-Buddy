/**
 * @file screen_menu_leds.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"
#include <option/has_toolchanger.h>
#include "MItem_menus.hpp"

using ScreenMenuLeds__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
#if HAS_LEDS()
    MI_LEDS_ENABLE,
    MI_STATUS_LED_COLOR_SETTINGS,
#endif
#if HAS_TOOLCHANGER()
    MI_TOOL_LEDS_ENABLE,
#endif
#if HAS_SIDE_LEDS()
    MI_LED_DEEP_IDLE_SETTINGS,
    MI_LED_IDLE_SETTINGS,
    MI_LED_ACTIVE_SETTINGS,
    MI_LED_PRINTING_SETTINGS,
    MI_POST_PRINT_LED_HOLD,
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    MI_EXTERNAL_LIGHT_BAR_SETTINGS,
#endif
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLeds : public ScreenMenuLeds__ {
public:
    ScreenMenuLeds();
};

/// @file
#pragma once

#include "MItem_tools.hpp"
#include "MItem_menus.hpp"
#include <basic_screen_menu.hpp>
#include <option/has_leds.h>
#include <option/has_side_leds.h>
#include <option/has_toolchanger.h>

using ScreenMenuLightsBase = BasicScreenMenu<
    MI_LED_DEEP_IDLE,
    MI_LED_IDLE,
    MI_LED_ACTIVE,
    MI_LED_PRINTING,
#if HAS_LEDS()
    MI_LEDS_ENABLE,
    MI_STATUS_LED_COLORS,
    MI_STATUS_LED_FINISHED_HOLD,
#endif
#if HAS_TOOLCHANGER()
    MI_TOOL_LEDS_ENABLE,
#endif
#if HAS_SIDE_LEDS()
    MI_CHAMBER_LIGHT_MODE,
    MI_SIDE_LEDS_ACTIVITY_TIMEOUT,
    MI_SIDE_LEDS_OFF_TIMEOUT,
    MI_POST_PRINT_LED_HOLD,
    MI_SIDE_LEDS_MAX_BRIGTHNESS,
    MI_SIDE_LEDS_DIMMING_ENABLE,
    MI_SIDE_LEDS_DIMMED_BRIGTHNESS,
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    MI_EXTERNAL_LIGHT_BAR,
#endif
    MI_ALWAYS_HIDDEN>;

class ScreenMenuLights final : public ScreenMenuLightsBase {
public:
    ScreenMenuLights();
};

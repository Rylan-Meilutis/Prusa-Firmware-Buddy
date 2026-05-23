/**
 * @file screen_menu_external_light_bar.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"

#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()

class MI_EXTERNAL_LIGHT_BAR_WIRING_1 : public WI_INFO_t {
    static constexpr const char *const label = N_("Wiring");

public:
    MI_EXTERNAL_LIGHT_BAR_WIRING_1();
};

class MI_EXTERNAL_LIGHT_BAR_WIRING_2 : public WI_INFO_t {
    static constexpr const char *const label = N_("Power");

public:
    MI_EXTERNAL_LIGHT_BAR_WIRING_2();
};

class MI_EXTERNAL_LIGHT_BAR_WIRING_3 : public WI_INFO_t {
    static constexpr const char *const label = N_("Output");

public:
    MI_EXTERNAL_LIGHT_BAR_WIRING_3();
};

class MI_EXTERNAL_LIGHT_BAR_WIRING_4 : public WI_INFO_t {
    static constexpr const char *const label = N_("Ground");

public:
    MI_EXTERNAL_LIGHT_BAR_WIRING_4();
};

class MI_EXTERNAL_LIGHT_BAR_WIRING_5 : public WI_INFO_t {
    static constexpr const char *const label = N_("Protect");

public:
    MI_EXTERNAL_LIGHT_BAR_WIRING_5();
};

using ScreenMenuExternalLightBar__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    MI_EXTERNAL_LIGHT_BAR_WIRING_1,
    MI_EXTERNAL_LIGHT_BAR_WIRING_2,
    MI_EXTERNAL_LIGHT_BAR_WIRING_3,
    MI_EXTERNAL_LIGHT_BAR_WIRING_4,
    MI_EXTERNAL_LIGHT_BAR_WIRING_5,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 0>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 1>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 2>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 3>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 4>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 5>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 6>,
    WithConstructorArgs<MI_EXTERNAL_LIGHT_BAR_PIN_MODE, 7>,
    MI_ALWAYS_HIDDEN>;

class ScreenMenuExternalLightBar : public ScreenMenuExternalLightBar__ {
public:
    constexpr static const char *label = N_("External Light Bar");
    ScreenMenuExternalLightBar();
};

#endif

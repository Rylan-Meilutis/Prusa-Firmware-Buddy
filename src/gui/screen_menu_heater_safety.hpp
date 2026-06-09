#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"

using ScreenMenuHeaterSafety__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    MI_HOTEND_HEATER_SAFETY_TIMEOUT,
    MI_BED_HEATER_SAFETY_TIMEOUT>;

class ScreenMenuHeaterSafety : public ScreenMenuHeaterSafety__ {
public:
    ScreenMenuHeaterSafety();
};

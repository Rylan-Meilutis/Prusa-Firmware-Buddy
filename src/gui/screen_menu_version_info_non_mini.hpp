/// @file
#pragma once

#include <basic_screen_menu.hpp>
#include "MItem_menus.hpp"
#include "MItem_tools.hpp"

using ScreenMenuVersionInfo__ = BasicScreenMenu<
    MI_INFO_FW,
    MI_INFO_BOOTLOADER,
    MI_INFO_MMU,
    MI_BOARD_INFO>;

class ScreenMenuVersionInfo final : public ScreenMenuVersionInfo__ {
public:
    ScreenMenuVersionInfo();
};

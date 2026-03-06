/**
 * @file screen_menu_tools.hpp
 * @brief This is temporary menu enabling dock position and tool offset view and edit. Simple manual calibration of the dock position is included.
 */

#pragma once

#include "screen_menu.hpp"
#include "MItem_tools.hpp"
#include "MItem_hardware.hpp"

class MI_INFO_DWARF_BOARD_TEMPERATURE : public MenuItemAutoUpdatingLabel<float> {
public:
    MI_INFO_DWARF_BOARD_TEMPERATURE();
};

class MI_INFO_DWARF_MCU_TEMPERATURE : public MenuItemAutoUpdatingLabel<float> {
public:
    MI_INFO_DWARF_MCU_TEMPERATURE();
};

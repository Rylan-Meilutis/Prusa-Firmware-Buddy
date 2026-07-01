/**
 * @file screen_menu_user_interface.cpp
 */

#include "screen_menu_user_interface.hpp"
#include "gcode_info.hpp"

ScreenMenuUserInterface::ScreenMenuUserInterface()
    : ScreenMenuUserInterface__(_(label)) {
    EnableLongHoldScreenAction();
}

/**
 * @file screen_menu_system.cpp
 */
#include "screen_menu_system.hpp"

#include <option/has_e2ee_support.h>

ScreenMenuSystem::ScreenMenuSystem()
    : ScreenMenuSystem__(_(label)) {
    EnableLongHoldScreenAction();
}

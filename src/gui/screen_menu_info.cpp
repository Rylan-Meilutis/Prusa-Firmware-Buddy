/**
 * @file screen_menu_info.cpp
 */

#include "screen_menu_info.hpp"
#include "img_resources.hpp"

ScreenMenuInfo::ScreenMenuInfo()
    : ScreenMenuInfo__(_(label)) {
    EnableLongHoldScreenAction();
    header.SetIcon(&img::info_16x16);
}

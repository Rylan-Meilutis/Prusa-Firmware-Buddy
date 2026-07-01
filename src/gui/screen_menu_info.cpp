/**
 * @file screen_menu_info.cpp
 */

#include "screen_menu_info.hpp"
#include "img_resources.hpp"

ScreenMenuInfo::ScreenMenuInfo()
    : ScreenMenuInfo__(_(label)) {
    EnableLongHoldScreenAction();
#if (!PRINTER_IS_PRUSA_MINI())
    header.SetIcon(&img::info_16x16);
#endif // PRINTER_IS_PRUSA_MINI()
}

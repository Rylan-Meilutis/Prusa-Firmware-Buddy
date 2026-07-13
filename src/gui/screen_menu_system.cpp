/// @file
#include <screen_menu_system.hpp>

ScreenMenuSystem::ScreenMenuSystem()
    : ScreenMenuSystemBase {
        _("SYSTEM"),
    } {
    EnableLongHoldScreenAction();
}

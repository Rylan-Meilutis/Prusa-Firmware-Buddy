/// @file
#include "screen_menu_user_interface.hpp"

ScreenMenuUserInterface::ScreenMenuUserInterface()
    : ScreenMenuUserInterface__ {
        _("USER INTERFACE"),
    } {
    EnableLongHoldScreenAction();
}

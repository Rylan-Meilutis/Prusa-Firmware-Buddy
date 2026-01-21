/// @file
#include "screen_menu_filament.hpp"

ScreenMenuFilament::ScreenMenuFilament()
    : ScreenMenuFilament__(_(label)) {
#if (!PRINTER_IS_PRUSA_MINI())
    header.SetIcon(&img::spool_white_16x16);
#endif // PRINTER_IS_PRUSA_MINI()
}

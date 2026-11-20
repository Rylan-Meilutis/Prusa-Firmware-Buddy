#pragma once

#include <manual_motion_safety.hpp>
#include <option/has_indx.h>

// Include after the machine configuration. Shared by the menu and G123 so
// service travel cannot be enabled just by changing the command's origin.
namespace buddy::manual_motion_safety {
inline Range x_range() {
#if HAS_INDX()
    return { X_MIN_POS, X_NOZZLE_CLEANER_ORIGIN - 10.35f };
#else
    return { X_MIN_POS, X_MAX_POS };
#endif
}
inline Range y_range() {
#if HAS_INDX()
    return { Y_DOCK_PARKING_MIN_SAFE_POS, Y_MAX_POS };
#elif PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX()
    return { Y_MIN_POS, Y_MAX_PRINT_POS };
#else
    return { Y_MIN_POS, Y_MAX_POS };
#endif
}

// These are operator-jog references, NOT evidence of homing. INDX's unsafe
// docks/cleaner are beyond the manual Y-min/X-max boundaries. Rear-dock
// models start at the rear so X remains blocked until Y leaves the docks.
inline float assumed_x_position() {
#if HAS_INDX()
    return x_range().max;
#else
    return x_range().min;
#endif
}
inline float assumed_y_position() {
#if PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX()
    return Y_MAX_POS;
#else
    return y_range().min;
#endif
}
} // namespace buddy::manual_motion_safety

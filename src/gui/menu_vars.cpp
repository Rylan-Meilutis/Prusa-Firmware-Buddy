#include "menu_vars.h"
#include "config.h"
#include "int_to_cstr.h"

#include "../Marlin/src/module/temperature.h"
#include <manual_motion_limits.hpp>

const std::pair<int, int> MenuVars::crash_sensitivity_range = {
#if AXIS_DRIVER_TYPE_X(TMC2209)
    0, 255
#elif AXIS_DRIVER_TYPE_X(TMC2130)
    -64, 63
#else
    #error "Unknown driver type."
#endif
};

std::pair<int, int> MenuVars::axis_range(uint8_t axis) {
    switch (axis) {
    case X_AXIS: {
        const auto range = buddy::manual_motion_safety::x_range();
        return { int(std::ceil(range.min)), int(std::floor(range.max)) };
    }

    case Y_AXIS: {
        const auto range = buddy::manual_motion_safety::y_range();
        return { int(std::ceil(range.min)), int(std::floor(range.max)) };
    }

    case Z_AXIS:
        return { static_cast<int>(std::lround(Z_MIN_POS)), static_cast<int>(get_z_max_pos_mm_rounded()) };

    case E_AXIS:
        return { -EXTRUDE_MAXLENGTH, EXTRUDE_MAXLENGTH };

    default:
        return { 0, 0 };
    }
}

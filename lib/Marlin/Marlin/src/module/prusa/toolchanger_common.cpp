/// @file
#include "toolchanger.h"

#include <module/planner.h>

void PrusaToolChanger::z_shift(const float diff) {
    if (axes_home_level.is_homed(Z_AXIS, AxisHomeLevel::imprecise)) {
        MachinePosXYZE target = current_machine_position();
        target.z += diff;
        target.z = std::clamp<float>(target.z, Z_MIN_POS, Z_MAX_POS);
        line_to_machine_pos(target, Z_HOP_FEEDRATE_MM_S);

    } else {
        // Prevent nasty motor skipping
        do_homing_move(Z_AXIS, diff, HOMING_FEEDRATE_INVERTED_Z);
    }

    planner.synchronize();
}

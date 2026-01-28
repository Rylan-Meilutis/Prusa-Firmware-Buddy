/// @file
#include "steady_state_hotend.hpp"

#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>

#if ENABLED(STEADY_STATE_HOTEND)

float steady_state_hotend(float target_temp, float print_fan) {
    static_assert(PID_MAX == 255, "PID_MAX == 255 expected");
    // TODO Square root computation can be mostly avoided by if stored and updated only on print_fan change
    const float tdiff = target_temp - ambient_temp;
    const float retval = (tdiff * STEADY_STATE_HOTEND_LINEAR_COOLING_TERM
                             + sq(tdiff) * STEADY_STATE_HOTEND_QUADRATIC_COOLING_TERM * (1 - print_fan))
        * SQRT(1 + print_fan * STEADY_STATE_HOTEND_FAN_COOLING_TERM);
    return _MAX(retval, 0);
}

#else

float steady_state_hotend(float, float) {
    return 0;
}

#endif

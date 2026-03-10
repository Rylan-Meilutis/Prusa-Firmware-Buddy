#include <fanctl/CFanCtlCommon.hpp>

bool CFanCtlCommon::is_fan_ok() const {
    if (selftest_mode || get_pwm() == 0) {
        return true;
    }

    const auto state = get_state();
    return get_rpm_is_ok() || (state != running && state != error_running && state != error_starting);
}

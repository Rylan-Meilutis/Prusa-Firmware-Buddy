#include "src/common/m976_dock_fan_policy.hpp"

using buddy::m976_dock_fan_policy::pwm;
static_assert(pwm(0, 300, 0) == 0);
static_assert(pwm(1, 300, 0) == 0);
static_assert(pwm(1, 240, 73) == 73);
static_assert(pwm(2, 170, 0) == 128);
static_assert(pwm(2, 220, 0) == 176);
static_assert(pwm(2, 240, 0) == 196);
static_assert(pwm(8, 300, 0) == 255);
static_assert(pwm(2, 120, 0) == 128);
static_assert(pwm(2, 350, 0) == 255);
static_assert(pwm(2, 220, 220) == 220);
static_assert(pwm(2, 240, 255) == 255);
int main() {
    for (int t = 170; t < 300; ++t) {
        if (pwm(2, t, 0) > pwm(2, t + 1, 0)) {
            return 1;
        }
    }
}

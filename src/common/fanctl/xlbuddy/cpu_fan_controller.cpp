#include "cpu_fan_controller.hpp"
#include <option/has_cpu_fan.h>

static_assert(HAS_CPU_FAN());

#include <fanctl.hpp>

namespace cpu_fan_controller {

static_assert(compute_pwm(temp_off, 0, 0) == 0, "invalid PWM for temp_off");
static_assert(compute_pwm(0, temp_off, 0) == 0, "invalid PWM for temp_off");
static_assert(compute_pwm(temp_full, 0, 0) == 255, "invalid PWM for temp_full");
static_assert(compute_pwm(0, temp_full, 0) == 255, "invalid PWM for temp_full");

void update(float mcu_temp_c, float sandwich_temp_c) {
    Fans::cpu().set_pwm(compute_pwm(mcu_temp_c, sandwich_temp_c, Fans::cpu().get_pwm()));
}

} // namespace cpu_fan_controller

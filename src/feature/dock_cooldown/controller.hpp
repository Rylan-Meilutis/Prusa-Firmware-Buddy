#pragma once

#include <cstdint>

namespace buddy::dock_cooldown {
// All entry points run on the Marlin task, including step() from cycle().
void configure(bool enabled, uint32_t seconds);
void request_pwm(uint16_t pwm);
uint8_t requested_pwm();
void before_park(uint8_t tool);
void parked(uint8_t tool);
void picked(uint8_t tool);
void step();
void report();
} // namespace buddy::dock_cooldown

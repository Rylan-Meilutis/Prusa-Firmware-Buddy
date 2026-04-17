//@file
#pragma once

#include <cstdint>

#include <indx_head/leds.hpp>

namespace app {
void run();
int16_t get_nozzle_temp();
void set_led_config(const indx_head::leds::LedConfig cfg);
} // namespace app

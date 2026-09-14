/// @file
#pragma once

#include <utils/led_color.hpp>

namespace indx_head_leds {

/// Mirrors the already-rendered center pixel of the front status bar to the INDX
/// head. This keeps configured colors, brightness and animation frames identical.
/// Respects config_store().tool_leds_enabled and only sends real changes.
void update(leds::ColorRGBW front_status_color);

} // namespace indx_head_leds

/// @file
#pragma once

/// @brief Get steady state output needed to compensate hotend cooling
///
/// @param target_temp target temperature in degrees Celsius
/// @param print_fan print fan power in range 0.0 .. 1.0
/// @return hotend PWM in range 0 .. 255
float steady_state_hotend(float target_temp, float print_fan);

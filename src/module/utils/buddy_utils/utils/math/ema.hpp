/// @file
#pragma once

#include <cmath>
#include <algorithm>

/// Calculates exponential moving average
/// Uses expf approximation to speed up computation
/// Keep delta_time below 0.5 and tau above 0.5.
inline float exponential_moving_average(float current, float target, float delta_time, float tau) {
    // Actual expf is expensive. Use third order Taylor series approximation expanded through Horner's method
    constexpr auto expf_approx = [](float x) {
        return 1.f + x * (1.f + x * (0.5f + x * (1.f / 6.f)));
    };

    // Clamp alpha to prevent nonsense values caused by the approximation
    const float alpha = std::clamp<float>(1.0f - expf_approx(-delta_time / tau), 0, 1);

    return alpha * target + (1.f - alpha) * current;
}

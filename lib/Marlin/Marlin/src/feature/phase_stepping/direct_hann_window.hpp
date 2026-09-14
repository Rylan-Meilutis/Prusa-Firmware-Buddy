#pragma once

#include <cmath>
#include <numbers>
#include <tuple>

namespace phase_stepping {

template <typename Sample>
float direct_rectangular_window_magnitude(const int half_size, Sample &&sample) {
    const int size = half_size * 2 + 1;
    if (size <= 0) {
        return 0;
    }
    float sin_sum = 0;
    float cos_sum = 0;
    for (int i = 0; i < size; ++i) {
        const auto [sin_value, cos_value] = sample(i - half_size + 1);
        sin_sum += sin_value;
        cos_sum += cos_value;
    }
    return std::sqrt(sin_sum * sin_sum + cos_sum * cos_sum) * 2 / size;
}

/// Compute Hann-windowed correlation power without temporary heap storage.
/// sample(relative_index) returns the sine and cosine correlation components.
template <typename Sample>
float direct_hann_window_power(const int half_size, Sample &&sample) {
    const int size = half_size * 2 + 1;
    if (size <= 1) {
        return 0;
    }

    float sin_sum = 0;
    float cos_sum = 0;
    for (int i = 0; i < size; ++i) {
        const auto [sin_value, cos_value] = sample(i - half_size + 1);
        const float x = 2 * std::numbers::pi_v<float> * i / (size - 1);
        const float weight = 0.5f * (1 - std::cos(x));
        sin_sum += sin_value * weight;
        cos_sum += cos_value * weight;
    }
    return sin_sum * sin_sum + cos_sum * cos_sum;
}

} // namespace phase_stepping

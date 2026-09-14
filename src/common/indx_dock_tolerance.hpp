#pragma once

#include <algorithm>
#include <cmath>

namespace indx_dock_tolerance {

inline constexpr float default_x_mm = 2.5f;
inline constexpr float default_y_mm = 1.0f;
inline constexpr float minimum_mm = 0.5f;
inline constexpr float maximum_mm = 5.0f;

inline float sanitize(const float value, const float fallback) {
    return std::isfinite(value) ? std::clamp(value, minimum_mm, maximum_mm) : fallback;
}

inline bool accepts_offset(const float delta_x, const float delta_y, const float tolerance_x, const float tolerance_y, const float epsilon = 0.05f) {
    return std::isfinite(delta_x) && std::isfinite(delta_y)
        && std::abs(delta_x) <= sanitize(tolerance_x, default_x_mm) + epsilon
        && std::abs(delta_y) <= sanitize(tolerance_y, default_y_mm) + epsilon;
}

} // namespace indx_dock_tolerance

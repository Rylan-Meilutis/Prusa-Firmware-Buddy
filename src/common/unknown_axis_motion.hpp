#pragma once

#include <algorithm>

namespace buddy::unknown_axis_motion {

enum class AssumedBoundary { minimum,
    maximum };

constexpr float assumed_position(AssumedBoundary boundary, float minimum, float maximum) {
    return boundary == AssumedBoundary::minimum ? minimum : maximum;
}

constexpr float constrain(float destination, float minimum, float maximum) {
    return std::clamp(destination, minimum, maximum);
}

} // namespace buddy::unknown_axis_motion

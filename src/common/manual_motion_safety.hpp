#pragma once

#include <cmath>

namespace buddy::manual_motion_safety {
struct Range {
    float min, max;
};

// Unknown positions are not a license to jog from an invented home corner.
// A homed axis slightly outside its manual range may only move back toward it.
inline bool axis_move_is_safe(float from, float to, Range range) {
    if (!std::isfinite(from) || !std::isfinite(to)) {
        return false;
    }
    if (from == to) {
        return true;
    }
    if (from < range.min) {
        return to > from && to <= range.max;
    }
    if (from > range.max) {
        return to < from && to >= range.min;
    }
    return to >= range.min && to <= range.max;
}
} // namespace buddy::manual_motion_safety

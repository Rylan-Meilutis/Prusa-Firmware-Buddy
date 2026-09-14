#pragma once

namespace buddy::indx_serial_motion_safety {

struct Bounds {
    float min_x, max_x, min_y, max_y;
};

constexpr bool point_is_safe(const float x, const float y, const Bounds bounds) {
    return x >= bounds.min_x && x <= bounds.max_x
        && y >= bounds.min_y && y <= bounds.max_y;
}

// Conservatively require the complete circle to fit. This protects full-circle
// commands and does not trust a malformed sweep direction near service hardware.
constexpr bool arc_is_safe(const float center_x, const float center_y, const float radius, const Bounds bounds) {
    return radius >= 0
        && point_is_safe(center_x - radius, center_y - radius, bounds)
        && point_is_safe(center_x + radius, center_y + radius, bounds);
}

} // namespace buddy::indx_serial_motion_safety

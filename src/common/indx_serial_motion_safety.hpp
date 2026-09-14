#pragma once

namespace buddy::indx_serial_motion_safety {

struct ServiceBoundary {
    float cleaner_min_x;
    float dock_max_y;
};

constexpr bool point_is_safe(const float x, const float y, const ServiceBoundary boundary) {
    return x <= boundary.cleaner_min_x && y >= boundary.dock_max_y;
}

// Conservatively require the complete circle to fit. This protects full-circle
// commands and does not trust a malformed sweep direction near service hardware.
constexpr bool arc_is_safe(const float center_x, const float center_y, const float radius, const ServiceBoundary boundary) {
    return radius >= 0
        && center_x + radius <= boundary.cleaner_min_x
        && center_y - radius >= boundary.dock_max_y;
}

} // namespace buddy::indx_serial_motion_safety

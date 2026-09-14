#pragma once

namespace buddy::indx_serial_motion_safety {

struct ServiceBoundary {
    float cleaner_min_x;
    float dock_max_y;
};

constexpr bool point_is_safe(const float x, const float y, const ServiceBoundary boundary) {
    return x <= boundary.cleaner_min_x && y >= boundary.dock_max_y;
}

// The front service strip may be entered with a straight Y move (the slicer
// uses this for its priming line).  What is unsafe there is lateral carriage
// travel, which can sweep through the parked tools or ventilation lever.  The
// cleaner remains an independent hard X boundary for host-originated moves.
constexpr bool linear_move_is_safe(const float from_x, const float from_y,
    const float to_x, const float to_y, const ServiceBoundary boundary) {
    if (from_x > boundary.cleaner_min_x || to_x > boundary.cleaner_min_x) {
        return false;
    }
    constexpr float unchanged_epsilon = 0.0001f;
    const bool lateral_move = from_x < to_x - unchanged_epsilon || from_x > to_x + unchanged_epsilon;
    return !lateral_move || (from_y >= boundary.dock_max_y && to_y >= boundary.dock_max_y);
}

// Conservatively require the complete circle to fit. This protects full-circle
// commands and does not trust a malformed sweep direction near service hardware.
constexpr bool arc_is_safe(const float center_x, const float center_y, const float radius, const ServiceBoundary boundary) {
    return radius >= 0
        && center_x + radius <= boundary.cleaner_min_x
        && center_y - radius >= boundary.dock_max_y;
}

} // namespace buddy::indx_serial_motion_safety

#pragma once

#include <cmath>

namespace buddy::indx_serial_motion_safety {

struct ServiceBoundary {
    float cleaner_min_x;
    float dock_max_y;
};

// Native G12 leaves the nozzle in this lane for slicer-controlled purging.
// Both endpoints must already be in the lane; this cannot authorize entry
// across the cleaner wall or lateral travel toward parked tools.
constexpr bool cleaner_purge_move_is_safe(float from_x, float from_y,
    float to_x, float to_y, bool x_requested, bool z_requested) {
    return !x_requested && !z_requested
        && from_x >= -0.5f && from_x <= 0.5f
        && to_x >= -0.5f && to_x <= 0.5f
        && from_y >= 76.f && from_y <= 101.5f
        && to_y >= 76.f && to_y <= 101.5f;
}

constexpr bool point_is_safe(const float x, const float y, const ServiceBoundary boundary) {
    return x <= boundary.cleaner_min_x && y >= boundary.dock_max_y;
}

// The front service strip may be entered with a straight Y move (the slicer
// uses this for its priming line).  What is unsafe there is lateral carriage
// travel, which can sweep through the parked tools or ventilation lever.  The
// cleaner remains an independent hard X boundary for host-originated moves.
constexpr bool linear_move_is_safe(const float from_x, const float from_y,
    const float to_x, const float to_y, const bool x_move_requested, const ServiceBoundary boundary) {
    if (from_x > boundary.cleaner_min_x || to_x > boundary.cleaner_min_x) {
        return false;
    }
    // Decide from the command, not transformed coordinates. Tool offsets and
    // mesh transforms can introduce a small apparent X delta into a Y-only
    // priming move even though the host did not request lateral travel.
    return !x_move_requested || (from_y >= boundary.dock_max_y && to_y >= boundary.dock_max_y);
}

// Conservatively require the complete circle to fit. This protects full-circle
// commands and does not trust a malformed sweep direction near service hardware.
constexpr bool arc_is_safe(const float center_x, const float center_y, const float radius, const ServiceBoundary boundary) {
    return radius >= 0
        && center_x + radius <= boundary.cleaner_min_x
        && center_y - radius >= boundary.dock_max_y;
}

// Check only extrema on the commanded sweep, not the unused part of its circle.
// full_circle must use the planner's endpoint-coincidence rule.
inline bool arc_sweep_is_safe(float from_x, float from_y, float to_x, float to_y,
    float center_x, float center_y, bool clockwise, bool full_circle, ServiceBoundary boundary) {
    if (!std::isfinite(from_x) || !std::isfinite(from_y)
        || !std::isfinite(to_x) || !std::isfinite(to_y)
        || !std::isfinite(center_x) || !std::isfinite(center_y)
        || !point_is_safe(from_x, from_y, boundary) || !point_is_safe(to_x, to_y, boundary)) {
        return false;
    }
    const float sx = from_x - center_x, sy = from_y - center_y;
    const float ex = to_x - center_x, ey = to_y - center_y;
    const float radius = std::hypot(sx, sy);
    if (!std::isfinite(radius) || radius == 0) {
        return false;
    }
    if (full_circle) {
        return arc_is_safe(center_x, center_y, radius, boundary);
    }
    constexpr float tau = 6.2831853071795864769f;
    const auto directed_angle = [clockwise](float angle) {
        if (clockwise) {
            angle = -angle;
        }
        return angle < 0 ? angle + tau : angle;
    };
    const float sweep = directed_angle(std::atan2(sx * ey - sy * ex, sx * ex + sy * ey));
    // Match plan_arc: a zero angular travel queues no arc.
    if (sweep == 0) {
        return true;
    }
    const float to_right = directed_angle(std::atan2(-sy, sx));
    const float to_bottom = directed_angle(std::atan2(-sx, -sy));
    // Include the ideal circular endpoint as well as the commanded endpoint:
    // rounded G-code endpoints can have slightly different radii.
    const float end_radius = std::hypot(ex, ey);
    if (!(end_radius > 0) || !std::isfinite(end_radius)) {
        return false;
    }
    if (!point_is_safe(center_x + radius * ex / end_radius,
            center_y + radius * ey / end_radius, boundary)) {
        return false;
    }
    return (to_right > sweep || center_x + radius <= boundary.cleaner_min_x)
        && (to_bottom > sweep || center_y - radius >= boundary.dock_max_y);
}

} // namespace buddy::indx_serial_motion_safety

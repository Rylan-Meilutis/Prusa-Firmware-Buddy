#pragma once

#include <cmath>
#include <initializer_list>
#include <string_view>

namespace buddy::manual_motion_safety {
struct Range {
    float min, max;
};

// A queued display/temperature query must not cancel an LCD jog. Anything
// outside this small read-only set still yields promptly, including a new jog.
inline bool jog_telemetry(std::string_view command) {
    const auto trim = [&] {
        const auto first = command.find_first_not_of(" \t");
        command = first == std::string_view::npos ? std::string_view {} : command.substr(first);
    };
    trim();
    if (command.starts_with('N')) {
        command.remove_prefix(1);
        while (!command.empty() && command.front() >= '0' && command.front() <= '9') {
            command.remove_prefix(1);
        }
        trim();
    }
    for (const auto code : { std::string_view("M73"), std::string_view("M105"), std::string_view("M114"), std::string_view("M117") }) {
        if (command.starts_with(code)
            && (command.size() == code.size() || command[code.size()] == ' ' || command[code.size()] == '\t' || command[code.size()] == '*')) {
            return true;
        }
    }
    return false;
}

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

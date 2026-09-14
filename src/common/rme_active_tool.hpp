#pragma once

#include <cstdint>
#include <variant>

namespace rme_active_tool {

struct Snapshot {
    bool selected;
    uint8_t index;
};

template <typename Tool, typename NoToolValue>
inline Snapshot snapshot(const std::variant<Tool, NoToolValue> &tool) {
    if (const auto *selected = std::get_if<Tool>(&tool)) {
        return { true, selected->to_raw() };
    }
    return { false, 0 };
}

} // namespace rme_active_tool

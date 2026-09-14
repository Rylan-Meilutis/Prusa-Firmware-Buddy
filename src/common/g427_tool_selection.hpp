#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace g427_tool_selection {

struct ParseResult {
    bool present {};
    std::optional<uint32_t> mask;
};

/// Parse the optional comma-separated physical-tool list in `G427 T0,7 ...`.
constexpr ParseResult parse(std::string_view command, uint8_t tool_count) {
    const size_t marker = command.find(" T");
    if (marker == std::string_view::npos) {
        return {};
    }

    size_t cursor = marker + 2;
    uint32_t mask = 0;
    while (true) {
        if (cursor >= command.size() || command[cursor] < '0' || command[cursor] > '9') {
            return { true, std::nullopt };
        }
        unsigned tool = 0;
        while (cursor < command.size() && command[cursor] >= '0' && command[cursor] <= '9') {
            tool = tool * 10 + unsigned(command[cursor++] - '0');
        }
        if (tool >= tool_count || tool >= 32 || (mask & (uint32_t { 1 } << tool))) {
            return { true, std::nullopt };
        }
        mask |= uint32_t { 1 } << tool;
        if (cursor >= command.size() || command[cursor] != ',') {
            break;
        }
        ++cursor;
    }
    if (cursor < command.size() && command[cursor] != ' ' && command[cursor] != '*') {
        return { true, std::nullopt };
    }
    return { true, mask };
}

} // namespace g427_tool_selection

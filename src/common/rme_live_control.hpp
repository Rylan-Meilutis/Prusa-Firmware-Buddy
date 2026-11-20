#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace rme_live_control {
enum class Kind { bed,
    nozzle,
    fan,
    dock_fan,
    speed,
    flow,
    chamber };
struct Request {
    Kind kind;
    uint16_t value;
    std::optional<uint8_t> tool;
};

// Strict bounded parser: one setting, optional direct tool, no duplicate or
// unknown fields. Never forward arbitrary G-code into the priority channel.
inline std::optional<Request> parse(std::string_view args, bool chamber_only = false) {
    std::optional<Kind> kind;
    uint16_t value = 0;
    std::optional<uint8_t> tool;
    while (!args.empty()) {
        const auto end = args.find(' ');
        const auto token = args.substr(0, end);
        args = end == std::string_view::npos ? std::string_view {} : args.substr(end + 1);
        if (token.empty()) {
            continue;
        }
        const auto equal = token.find('=');
        if (equal == std::string_view::npos || equal + 1 == token.size()) {
            return {};
        }
        const auto name = token.substr(0, equal), number = token.substr(equal + 1);
        uint16_t parsed = 0;
        for (const char digit : number) {
            if (digit < '0' || digit > '9' || parsed > 500) {
                return {};
            }
            parsed = parsed * 10 + digit - '0';
        }
        if (parsed > 500) {
            return {};
        }
        if (name == "tool") {
            if (tool || parsed > 255) {
                return {};
            }
            tool = static_cast<uint8_t>(parsed);
            continue;
        }
        if (kind) {
            return {};
        }
        if (name == "bed") {
            kind = Kind::bed;
        } else if (name == "nozzle") {
            kind = Kind::nozzle;
        } else if (name == "fan") {
            kind = Kind::fan;
        } else if (name == "dock_fan") {
            kind = Kind::dock_fan;
        } else if (name == "speed") {
            kind = Kind::speed;
        } else if (name == "flow") {
            kind = Kind::flow;
        } else if (name == "chamber") {
            kind = Kind::chamber;
        } else {
            return {};
        }
        value = parsed;
    }
    if (!kind || (chamber_only != (*kind == Kind::chamber))) {
        return {};
    }
    if (tool && *kind != Kind::nozzle && *kind != Kind::flow) {
        return {};
    }
    switch (*kind) {
    case Kind::bed:
        if (value > 150) {
            return {};
        }
        break;
    case Kind::nozzle:
        break; // Native heater setter enforces machine limits.
    case Kind::fan:
    case Kind::dock_fan:
    case Kind::chamber:
        if (value > 255) {
            return {};
        }
        break;
    case Kind::speed:
        if (value < 10 || value > 300) {
            return {};
        }
        break;
    case Kind::flow:
        if (value < 50 || value > 150) {
            return {};
        }
        break;
    }
    return Request { *kind, value, tool };
}
} // namespace rme_live_control

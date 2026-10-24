#pragma once
#include <m976_diagnostics.hpp>
#include <string_view>

namespace rme_passive_command {
// No allocation, and distinguish M976 I from the I inside a HIPS manifest.
inline bool keeps_idle(std::string_view command) {
    command = command.substr(0, command.find_first_of(";*"));
    const auto trim = [&] {
        const auto first = command.find_first_not_of(" \t");
        command = first == std::string_view::npos ? std::string_view {} : command.substr(first);
    };
    trim();
    if (command.starts_with("N")) {
        command.remove_prefix(1);
        if (command.starts_with("-")) {
            command.remove_prefix(1);
        }
        while (!command.empty() && command.front() >= '0' && command.front() <= '9') {
            command.remove_prefix(1);
        }
        trim();
    }
    if (command.empty() || command.front() == '@') {
        return true;
    }
    if (command.front() != 'M') {
        return false;
    }
    command.remove_prefix(1);
    unsigned code = 0, digits = 0;
    while (!command.empty() && command.front() >= '0' && command.front() <= '9') {
        if (++digits > 4) {
            return false;
        }
        code = code * 10 + unsigned(command.front() - '0');
        command.remove_prefix(1);
    }
    if (!digits || command.starts_with(".")) {
        return false;
    }
    trim();
    switch (code) {
    case 27:
    case 73:
    case 105:
    case 114:
    case 115:
    case 117:
    case 119:
    case 153:
    case 155:
    case 503:
    case 865:
        return true;
    case 122:
        return command.empty(); // Driver diagnostic options can reset hardware.
    case 979:
        return command.empty(); // Only bare M979 is a passive cooldown query.
    case 976:
        return buddy::m976_diagnostics::inspection_requested(command);
    default:
        return false;
    }
}
} // namespace rme_passive_command

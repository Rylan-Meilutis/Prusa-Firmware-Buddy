#pragma once

#include <cstddef>
#include <cstdint>

namespace rme_spool_join {

constexpr bool valid_pair(const int32_t from, const int32_t to, const size_t tool_count) {
    return from >= 0 && to >= 0
        && static_cast<size_t>(from) < tool_count
        && static_cast<size_t>(to) < tool_count
        && from != to;
}

} // namespace rme_spool_join

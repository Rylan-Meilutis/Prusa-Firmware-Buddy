#pragma once

#include <cstddef>
#include <unordered_map>
#include <any>

struct VirtualToolIndex {
    uint8_t val;

    inline VirtualToolIndex(uint8_t val)
        : val(val) {}

    inline auto to_raw() const {
        return val;
    }
};

#pragma once

#include <cstdint>

namespace indx_head::leds {
enum class Mode : uint8_t {
    off,
    solid,
    pulse,
};

struct [[gnu::packed]] alignas(uint16_t) LedConfig {
    uint8_t r {};
    uint8_t g {};
    uint8_t b {};
    Mode mode {};

    bool operator==(const LedConfig &) const = default;
};
} // namespace indx_head::leds

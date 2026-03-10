/// @file
#pragma once

#include <cstdint>

enum class ToolType : uint8_t {
    /// No tool
    none,

    /// Classic FDM tool with a nozzle and blackjack
    standard_fff,
};

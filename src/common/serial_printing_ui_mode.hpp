#pragma once

#include <cstdint>

enum class SerialPrintingUiMode : uint8_t {
    legacy = 0,
    messages = 1,
    progress = 2,
    _last = progress,
};


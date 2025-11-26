#pragma once

#include <cstdint>

namespace modbus {

enum class ServerAddress : uint8_t {
    ac_controller = 0x1a + 8,
    mmu = 220,
    xbuddy_extension = 0x1a + 7,
};

}

/// @file
#pragma once

#include <cstdint>

namespace MMU2 {

enum class SerialHostMmuEvent : uint8_t {
    none = 0,
    paused = 1 << 0,
    resume = 1 << 1,
};

void defer_serial_host_mmu_paused();
void defer_serial_host_mmu_resume();
SerialHostMmuEvent consume_serial_host_mmu_events();

} // namespace MMU2

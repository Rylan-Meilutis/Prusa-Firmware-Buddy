#pragma once

#include <cstdint>
#include <ldc1612.hpp>

namespace hal {

enum class BoardOrientation : uint8_t {
    normal,
    left,
    right
};

/// Enable CAN bit rate switch?
static constexpr const bool enable_bit_rate_switch = false;

void init();
void set_status_led(bool set);
void ldc1612_set_enabled(bool enabled);

void __attribute__((noreturn)) panic();
void __attribute__((noreturn)) reset();

extern LDC1612 ldc1612;

} // namespace hal

#pragma once

#include <span>
#include <cstddef>

#include <option/can_bus_type.h>

namespace hal {

/// Enable CAN bit rate switch?
static constexpr const bool enable_bit_rate_switch =
#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_UART()
    false;
#elif CAN_BUS_TYPE_IS_SLX()
    true;
#else
    #error
#endif

void init();
void set_status_led(bool set);

void __attribute__((noreturn)) panic();
void __attribute__((noreturn)) reset();

} // namespace hal

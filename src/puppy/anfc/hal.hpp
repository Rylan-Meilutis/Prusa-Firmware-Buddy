#pragma once

#include <span>
#include <cstddef>

namespace hal {

void init();
void status_led_on();
void status_led_off();
void __attribute__((noreturn)) panic();

namespace memory {

    /// Memory address region dedicated to peripherals control
    extern const std::span<std::byte> peripheral_address_region;

}; // namespace memory

} // namespace hal

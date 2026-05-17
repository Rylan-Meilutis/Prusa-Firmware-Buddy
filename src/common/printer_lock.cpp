#include "printer_lock.hpp"

#include <config_store/store_instance.hpp>
#include <timing.h>

namespace printer_lock {
namespace {
bool is_locked = false;
uint32_t last_activity_ms = 0;
} // namespace

bool enabled() {
    return config_store().printer_lock_enabled.get() && config_store().printer_lock_pin_length.get() >= 4;
}

bool locked() {
    return enabled() && is_locked;
}

void lock() {
    if (enabled()) {
        is_locked = true;
    }
}

void unlock() {
    is_locked = false;
    record_activity();
}

void record_activity() {
    last_activity_ms = ticks_ms();
}

void loop() {
    if (!enabled()) {
        is_locked = false;
        record_activity();
        return;
    }

    const uint32_t timeout_ms = static_cast<uint32_t>(config_store().printer_lock_timeout_s.get()) * 1000;
    if (!is_locked && timeout_ms > 0 && static_cast<uint32_t>(ticks_diff(ticks_ms(), last_activity_ms)) >= timeout_ms) {
        is_locked = true;
    }
}

bool check_pin(uint32_t pin, uint8_t length) {
    return enabled() && length == config_store().printer_lock_pin_length.get() && pin == config_store().printer_lock_pin.get();
}

bool serial_commands_allowed() {
    return !locked() || config_store().printer_lock_accept_serial.get();
}

bool serial_print_start_allowed() {
    return serial_commands_allowed();
}

} // namespace printer_lock

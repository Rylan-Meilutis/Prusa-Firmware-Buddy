#include "printer_lock.hpp"

#include <config_store/store_instance.hpp>
#include <timing.h>
#include <atomic>

namespace printer_lock {
namespace {
    std::atomic<bool> is_locked { false };
    std::atomic<uint32_t> last_activity_ms { 0 };
} // namespace

bool enabled() {
    return config_store().printer_lock_enabled.get() && config_store().printer_lock_pin_length.get() >= 4;
}

bool locked() {
    return enabled() && is_locked.load(std::memory_order_acquire);
}

void lock() {
    if (enabled()) {
        is_locked.store(true, std::memory_order_release);
    }
}

void unlock() {
    is_locked.store(false, std::memory_order_release);
    record_activity();
}

void record_activity() {
    last_activity_ms.store(ticks_ms(), std::memory_order_release);
}

void loop() {
    if (!enabled()) {
        is_locked.store(false, std::memory_order_release);
        record_activity();
        return;
    }

    const uint32_t timeout_ms = static_cast<uint32_t>(config_store().printer_lock_timeout_s.get()) * 1000;
    if (!is_locked.load(std::memory_order_acquire) && timeout_ms > 0
        && static_cast<uint32_t>(ticks_diff(ticks_ms(), last_activity_ms.load(std::memory_order_acquire))) >= timeout_ms) {
        is_locked.store(true, std::memory_order_release);
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

#pragma once

#include <cstdint>

namespace printer_lock {

bool enabled();
bool locked();
void lock();
void unlock();
void record_activity();
void loop();
bool check_pin(uint32_t pin, uint8_t length);
bool serial_commands_allowed();
bool serial_print_start_allowed();

} // namespace printer_lock

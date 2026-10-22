#pragma once

#include <cstdint>

namespace buddy::serial_collision {
// All entry points run on the Marlin task. The ISR only snapshots the existing
// Crash_s planner metadata; it never touches strings, dialogs or the journal.
bool prepare(const char *command);
bool replay_one();
uint32_t gcode_position(uint32_t media_position);
bool begin();
void reset();
bool pending();
void suspend();
bool tool_unchanged();
bool monitoring_available();
} // namespace buddy::serial_collision

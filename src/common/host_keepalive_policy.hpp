#pragma once

#include <cstdint>

namespace buddy::host_keepalive_policy {

// Host keepalives acknowledge that a synchronous command is still making
// progress.  They must remain independent of temperature/status auto reports:
// calibrations intentionally suspend those reports while continuing to need a
// serial watchdog heartbeat.
constexpr bool should_emit(uint8_t interval_s, bool command_busy) {
    return interval_s != 0 && command_busy;
}

} // namespace buddy::host_keepalive_policy

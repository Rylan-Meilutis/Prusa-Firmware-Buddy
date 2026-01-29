/// @file
#pragma once

#include <cstdint>

#include <core/millis_t.h>
#include <wiring_time.h>

/// Class for sanity-checking the heaters (that they heat up in a required time)
class heater_watch_t {

public:
    /**
     * Checks if the next_ms has elapsed since the last call.
     * @retval true if ${next_ms} has elapsed and the watch is active. (next_ms != 0)
     * @retval false if ${next_ms} has not elapsed or the watch is inactive. (next_ms == 0)
     */
    inline bool elapsed(const millis_t &ms) {
        return (next_ms != 0) && ELAPSED(ms, next_ms);
    }
    inline bool elapsed() {
        return elapsed(millis());
    }

    // TODO private:
    /**
     * The target temperature that should be reached by the next check.
     * @warning: This value is not the same as temp_heatbreak.target
     */
    int16_t target = 0;

    /**
     * The time in milliseconds to wait for the temperature to rise.
     * @note: The value 0 means no watch.
     */
    millis_t next_ms = 0;
};

/// @file
#pragma once

#include <cstdint>

#include <core/millis_t.h>
#include <wiring_time.h>
#include <module/temperature/temp_defines.hpp>
#include <error_codes.hpp>

/// Class for sanity-checking the heaters (that they heat up in a required time)
class HeaterWatchBase {

public:
    struct Config {
        /// By how many degrees must the temperature increase
        /// within the @p period
        /// for the watch to not trigger
        int16_t temp_increase;

        /// The period for the temp to increase, in seconds
        uint16_t period_s;

        /// Minimum target-current temperature difference for the watch to engage
        int16_t min_temp_diff;

        /// Error code to raise if the watch fails
        ErrCode error_code;

        /// Inverts the checking logic
        /// !!! If true, temp_increase and min_temp_diff should probably be negative
        bool watch_cooling_instead = false;
    };

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

    inline bool is_running() const {
        return next_ms != 0;
    }

protected:
    void reset(const Config &config, float current_temp, int16_t target_temp);
    void check(const Config &config, float current_temp, int16_t target_temp);

private:
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

class HeaterWatch final : public HeaterWatchBase {

public:
    using HeaterWatchBase::check;
    using HeaterWatchBase::reset;
};

template <HeaterWatchBase::Config config>
class HeaterWatchWithConfig final : public HeaterWatchBase {

public:
    void reset(float current_temp, int16_t target_temp) {
        HeaterWatchBase::reset(config, current_temp, target_temp);
    }

    void check(float current_temp, int16_t target_temp) {
        HeaterWatchBase::check(config, current_temp, target_temp);
    }
};

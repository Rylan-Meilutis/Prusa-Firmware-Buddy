#pragma once

#include <cstdint>

#include <freertos/timing.hpp>

#define TIM_BASE_CLK_MHZ configTICK_RATE_HZ

inline int32_t ticks_diff(uint32_t ticks_a, uint32_t ticks_b) {
    return ((int32_t)(ticks_a - ticks_b));
}

inline uint32_t ticks_us() {
    // TODO: Actual microsecond counting
    return int64_t(freertos::millis()) * 1000;
}

inline uint32_t ticks_ms() {
    return freertos::millis();
}

inline int64_t get_timestamp_us() {
    return ticks_us();
}

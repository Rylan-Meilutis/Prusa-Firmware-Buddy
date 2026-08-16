#include <catch2/catch_test_macros.hpp>
#include <leds/light_state.hpp>

#include <cstdint>

TEST_CASE("Startup lighting remains active for a bounded window", "[leds][startup]") {
    constexpr uint32_t duration_ms = 30000;

    CHECK(leds::startup_activity_within_window(0, 0, duration_ms));
    CHECK(leds::startup_activity_within_window(0, duration_ms - 1, duration_ms));
    CHECK_FALSE(leds::startup_activity_within_window(0, duration_ms, duration_ms));
    CHECK_FALSE(leds::startup_activity_within_window(0, duration_ms + 1, duration_ms));
}

TEST_CASE("Startup lighting window survives the millisecond counter wrap", "[leds][startup]") {
    constexpr uint32_t started_ms = UINT32_MAX - 1000;
    constexpr uint32_t duration_ms = 30000;

    CHECK(leds::startup_activity_within_window(started_ms, 500, duration_ms));
    CHECK(leds::startup_activity_within_window(started_ms, 28998, duration_ms));
    CHECK_FALSE(leds::startup_activity_within_window(started_ms, 28999, duration_ms));
}

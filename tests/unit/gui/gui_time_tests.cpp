#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include "gui_time.hpp"

uint32_t tick = 0;

TEST_CASE("gui time test", "[gui_time]") {

    SECTION("init state") {
        REQUIRE(gui::GetTick() == 0);
}

SECTION("actualization") {
    tick = 100;
    gui::TickLoop();
    REQUIRE(gui::GetTick() == tick);

    tick = 200;
    gui::TickLoop();
    REQUIRE(gui::GetTick() == tick);
}
}
;

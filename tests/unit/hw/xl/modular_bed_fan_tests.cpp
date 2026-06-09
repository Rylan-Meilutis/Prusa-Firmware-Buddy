#include <catch2/catch_test_macros.hpp>

#include <modular_bed_fan.hpp>

using buddy::ModularBedFanControl;

TEST_CASE("ModularBedFanControl") {
    ModularBedFanControl fan;

    SECTION("Off below temp_on_c") {
        CHECK(fan.update(0) == 0);
        CHECK(fan.update(-5) == 0);
        CHECK(fan.update(49) == 0);
    }

    SECTION("Turns on at temp_on_c with min_pwm") {
        REQUIRE(fan.update(50) == ModularBedFanControl::min_pwm);
    }

    SECTION("max_pwm at and above temp_full_c") {
        CHECK(fan.update(65) == ModularBedFanControl::max_pwm);
        CHECK(fan.update(80) == ModularBedFanControl::max_pwm);
    }

    SECTION("Mid-ramp is monotonic") {
        const uint8_t pwm57 = fan.update(57);
        CHECK(pwm57 > ModularBedFanControl::min_pwm);
        CHECK(pwm57 < ModularBedFanControl::max_pwm);
        CHECK(fan.update(58) > pwm57);
    }

    SECTION("Hysteresis — stays on in 48-49 °C band once running") {
        CHECK(fan.update(55) > ModularBedFanControl::min_pwm);
        CHECK(fan.update(49) == ModularBedFanControl::min_pwm);
        CHECK(fan.update(48) == ModularBedFanControl::min_pwm);
    }

    SECTION("Hysteresis — turns off below temp_off_c") {
        CHECK(fan.update(55) > ModularBedFanControl::min_pwm);
        CHECK(fan.update(47) == 0);
    }

    SECTION("Cold start: update(49) does not turn on") {
        CHECK(fan.update(49) == 0);
    }

    SECTION("After cooling off, does not restart until temp_on_c") {
        CHECK(fan.update(55) > ModularBedFanControl::min_pwm);
        CHECK(fan.update(47) == 0); // dropped below temp_off_c: off
        CHECK(fan.update(49) == 0); // back in the band, but was off: stays off
        CHECK(fan.update(50) == ModularBedFanControl::min_pwm); // only restarts at temp_on_c
    }
}

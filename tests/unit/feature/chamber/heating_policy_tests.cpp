#include <catch2/catch_test_macros.hpp>
#include <feature/chamber/heating_policy.hpp>

using buddy::chamber_heating::assisted_output;
using buddy::chamber_heating::IdleBedAssist;
using buddy::chamber_heating::should_assist;

TEST_CASE("idle chamber bed ownership and hysteresis") {
    IdleBedAssist assist;
    CHECK_FALSE(assist.update(25, 40, false, 0, 110));
    assist.request();
    CHECK(assist.update(25, 40, false, 0, 110) == 80);
    CHECK_FALSE(assist.update(39.5f, 40, false, 80, 110));
    CHECK(assist.update(40, 40, false, 80, 110) == 0);
    CHECK_FALSE(assist.update(39.5f, 40, false, 0, 110));
    CHECK(assist.update(38, 40, false, 0, 110) == 80);
    CHECK(assist.update(38, 0, false, 80, 110) == 0);
}

TEST_CASE("print and explicit bed commands revoke chamber ownership") {
    IdleBedAssist assist;
    assist.request();
    CHECK(assist.update(25, 40, false, 60, 110) == 80);
    CHECK(assist.update(25, 40, true, 80, 110) == 60);
    CHECK_FALSE(assist.update(25, 40, false, 60, 110));
    assist.request();
    CHECK(assist.update(25, 40, false, 60, 110) == 80);
    assist.override_bed(); // Even a same-value M140/M190 transfers ownership.
    CHECK_FALSE(assist.update(25, 40, false, 80, 110));
    CHECK_FALSE(assist.update(40, 0, false, 80, 110));
    assist.request();
    CHECK(assist.update(25, 50, false, 80, 110) == 90);
    assist.override_bed(); // Safety timeout or explicit Off cannot be undone.
    CHECK_FALSE(assist.update(25, 50, false, 0, 110));
}

TEST_CASE("idle assistance respects limits and invalid sensors") {
    IdleBedAssist assist;
    assist.request();
    CHECK(assist.update(25, 65, false, 0, 90) == 90);
    CHECK(assist.update(std::nullopt, 65, false, 90, 90) == 0);
    CHECK_FALSE(assist.update(NAN, 65, false, 0, 90));
    CHECK_FALSE(assist.update(25, NAN, false, 0, 90));
    CHECK(assist.update(25, 65, false, 0, 120) == 100);
    CHECK_FALSE(assist.update(25, 65, false, 70, 120)); // External change wins.
    assist.request();
    CHECK_FALSE(assist.update(25, 40, false, 105, 120));
    CHECK_FALSE(assist.update(40, 40, false, 105, 120));
}

TEST_CASE("chamber heat assistance policy") {
    CHECK(should_assist(25.0f, 40.0f, false, false));
    CHECK_FALSE(should_assist(39.0f, 40.0f, false, false));
    CHECK_FALSE(should_assist(40.0f, 40.0f, false, false));
    CHECK_FALSE(should_assist(std::nullopt, 40.0f, false, false));
    CHECK_FALSE(should_assist(25.0f, std::nullopt, false, false));
    CHECK_FALSE(should_assist(25.0f, 0.0f, false, false));
}

TEST_CASE("printing owns chamber heating outputs except during M191") {
    CHECK_FALSE(should_assist(25.0f, 40.0f, true, false));
    CHECK(should_assist(25.0f, 40.0f, true, true));
}

TEST_CASE("chamber assistance never lowers an existing output") {
    CHECK(assisted_output<int16_t>(60, 120) == 120);
    CHECK(assisted_output<int16_t>(125, 120) == 125);
    CHECK(assisted_output<uint8_t>(0, 76) == 76);
    CHECK(assisted_output<uint8_t>(200, 76) == 200);
}

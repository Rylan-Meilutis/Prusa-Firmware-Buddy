#include <catch2/catch_test_macros.hpp>
#include <feature/chamber/heating_policy.hpp>

using buddy::chamber_heating::assisted_output;
using buddy::chamber_heating::should_assist;

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

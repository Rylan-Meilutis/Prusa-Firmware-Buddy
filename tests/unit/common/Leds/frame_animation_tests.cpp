#include "catch2/catch.hpp"
#include <led_animation_controller/frame_animation.hpp>

size_t freertos::millis() {
    return 0;
}

TEST_CASE("Solid frame animation scales percentages without wrapping") {
    using Animation = leds::FrameAnimation<3>;

    constexpr auto full = std::to_array<Animation::Frame>({ { 100, 100, 100 } });
    const Animation animation {
        {
            leds::ColorRGBW { 75, 46, 131, 255 },
            1000,
            0,
            300,
            full,
        },
    };

    const auto data = animation.render();
    for (const auto &color : data) {
        CHECK(color == leds::ColorRGBW { 75, 46, 131, 255 });
    }
}

TEST_CASE("Solid frame animation applies partial brightness") {
    using Animation = leds::FrameAnimation<3>;

    constexpr auto partial = std::to_array<Animation::Frame>({ { 0, 50, 100 } });
    const Animation animation {
        {
            leds::ColorRGBW { 128, 64, 32, 16 },
            1000,
            0,
            300,
            partial,
        },
    };

    const auto data = animation.render();
    CHECK(data[0] == leds::ColorRGBW { 0, 0, 0, 0 });
    CHECK(data[1] == leds::ColorRGBW { 64, 32, 16, 8 });
    CHECK(data[2] == leds::ColorRGBW { 128, 64, 32, 16 });
}

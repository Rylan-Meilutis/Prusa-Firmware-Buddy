#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <direct_hann_window.hpp>

#include <array>
#include <tuple>

TEST_CASE("Direct phase-stepping Hann analysis is allocation-free and ordered") {
    constexpr std::array<std::tuple<float, float>, 5> samples {
        std::tuple { 100.0f, 100.0f },
        std::tuple { 1.0f, 2.0f },
        std::tuple { 3.0f, 4.0f },
        std::tuple { 5.0f, 6.0f },
        std::tuple { 100.0f, 100.0f },
    };
    int calls = 0;
    const float power = phase_stepping::direct_hann_window_power(2, [&](const int relative_index) {
        ++calls;
        return samples[relative_index + 1];
    });

    // Five samples are visited. Hann weights are 0, .5, 1, .5, 0,
    // giving sine=6 and cosine=8.
    CHECK(calls == 5);
    CHECK(power == Catch::Approx(100.0f));
}

TEST_CASE("Direct phase-stepping rectangular analysis preserves window magnitude") {
    constexpr std::array<std::tuple<float, float>, 3> samples {
        std::tuple { 1.0f, 2.0f },
        std::tuple { 3.0f, 4.0f },
        std::tuple { 5.0f, 6.0f },
    };
    int calls = 0;
    const float magnitude = phase_stepping::direct_rectangular_window_magnitude(1, [&](const int relative_index) {
        ++calls;
        return samples[relative_index];
    });

    CHECK(calls == 3);
    CHECK(magnitude == Catch::Approx(std::sqrt(81.0f + 144.0f) * 2 / 3));
}

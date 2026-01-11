#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <signal_processing/pipeline.hpp>
#include <module/signal2step.hpp>
#include <vector>
#include <cmath>

// Helper structure to collect step events
struct StepRecord {
    uint32_t timestamp_us;
    AxisEnum axis;
    bool direction;
};

TEST_CASE("Signal2Step - Single axis forward movement", "[signal2step]") {
    using namespace sp::pipe;

    // Create a simple signal: A moves from 0mm to 4mm in 3 samples at 1000 Hz
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 2, 0, 0, 0 },
        { 4, 0, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    auto result = signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // Should generate 4 steps on X axis (from step 0 to step 4)
    REQUIRE(steps.size() == 4);

    for (const auto &step : steps) {
        REQUIRE(step.axis == X_AXIS);
        REQUIRE(step.direction == true); // Forward
    }

    // Steps should be sorted by timestamp
    for (size_t i = 1; i < steps.size(); ++i) {
        REQUIRE(steps[i].timestamp_us >= steps[i - 1].timestamp_us);
    }

    // Final position should be 4mm on A
    REQUIRE_THAT(result.a, Catch::Matchers::WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(result.b, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.c, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.e, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Signal2Step - Single axis backward movement", "[signal2step]") {
    using namespace sp::pipe;

    // B moves from 5mm to 2mm (3 steps backward)
    std::vector<abce_pos_t> data = {
        { 0, 5, 0, 0 },
        { 0, 2, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    auto result = signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // Should generate 3 steps on Y axis backward
    REQUIRE(steps.size() == 3);

    for (const auto &step : steps) {
        REQUIRE(step.axis == Y_AXIS);
        REQUIRE(step.direction == false); // Backward
    }
}

TEST_CASE("Signal2Step - Multi-axis movement with sorted timestamps", "[signal2step]") {
    using namespace sp::pipe;

    // Both A and B move simultaneously
    // A: 0 -> 3mm (3 steps)
    // B: 0 -> 2mm (2 steps)
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 3, 2, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // Should generate 5 steps total (3 on A, 2 on B)
    REQUIRE(steps.size() == 5);

    // Count steps per axis
    int a_steps = 0, b_steps = 0;
    for (const auto &step : steps) {
        if (step.axis == X_AXIS) {
            a_steps++;
        }
        if (step.axis == Y_AXIS) {
            b_steps++;
        }
    }

    REQUIRE(a_steps == 3);
    REQUIRE(b_steps == 2);

    // All steps should be in sorted order by timestamp
    for (size_t i = 1; i < steps.size(); ++i) {
        REQUIRE(steps[i].timestamp_us >= steps[i - 1].timestamp_us);
    }

    // All timestamps should be within the interval [0, 1000]us
    for (const auto &step : steps) {
        REQUIRE(step.timestamp_us <= 1000);
    }
}

TEST_CASE("Signal2Step - Interpolated timestamps", "[signal2step]") {
    using namespace sp::pipe;

    // A moves from 0 to 4mm in one interval
    // With 1mm per step, we expect 4 steps
    // At 1000Hz, interval is 1000us
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 4, 0, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    REQUIRE(steps.size() == 4);

    // Steps should be roughly evenly spaced
    // Expected at approximately: 125us, 375us, 625us, 875us
    // (when crossing 0.5, 1.5, 2.5, 3.5)
    REQUIRE(steps[0].timestamp_us < 200); // ~125us
    REQUIRE(steps[1].timestamp_us > 300);
    REQUIRE(steps[1].timestamp_us < 450); // ~375us
    REQUIRE(steps[2].timestamp_us > 550);
    REQUIRE(steps[2].timestamp_us < 700); // ~625us
    REQUIRE(steps[3].timestamp_us > 800); // ~875us
}

TEST_CASE("Signal2Step - No movement generates no steps", "[signal2step]") {
    using namespace sp::pipe;

    // All axes stay at 0
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    REQUIRE(steps.empty());
}

TEST_CASE("Signal2Step - Fractional movement less than one step", "[signal2step]") {
    using namespace sp::pipe;

    // A moves by 0.4mm (less than 1 step)
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 0.4f, 0, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // No steps should be generated (0.4mm rounds to 0 steps)
    REQUIRE(steps.empty());
}

TEST_CASE("Signal2Step - Different mm_per_step values", "[signal2step]") {
    using namespace sp::pipe;

    // A moves 10mm with 2mm per step = 5 steps
    // B moves 10mm with 1mm per step = 10 steps
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 10, 10, 0, 0 }
    };

    abce_pos_t mm_per_step = { 2, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // Count steps per axis
    int a_steps = 0, b_steps = 0;
    for (const auto &step : steps) {
        if (step.axis == X_AXIS) {
            a_steps++;
        }
        if (step.axis == Y_AXIS) {
            b_steps++;
        }
    }

    REQUIRE(a_steps == 5); // 10mm / 2mm per step
    REQUIRE(b_steps == 10); // 10mm / 1mm per step
}

TEST_CASE("Signal2Step - Extruder axis", "[signal2step]") {
    using namespace sp::pipe;

    // E (extruder) moves from 0 to 3mm
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 0, 0, 0, 3 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    REQUIRE(steps.size() == 3);

    for (const auto &step : steps) {
        REQUIRE(step.axis == E_AXIS);
        REQUIRE(step.direction == true);
    }
}

TEST_CASE("Signal2Step - All axes moving", "[signal2step]") {
    using namespace sp::pipe;

    // All axes move by 2mm
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 2, 2, 2, 2 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // Should generate 8 steps total (2 per axis * 4 axes)
    REQUIRE(steps.size() == 8);

    // Count steps per axis
    int a_steps = 0, b_steps = 0, c_steps = 0, e_steps = 0;
    for (const auto &step : steps) {
        if (step.axis == X_AXIS) {
            a_steps++;
        }
        if (step.axis == Y_AXIS) {
            b_steps++;
        }
        if (step.axis == Z_AXIS) {
            c_steps++;
        }
        if (step.axis == E_AXIS) {
            e_steps++;
        }
        REQUIRE(step.direction == true); // All forward
    }

    REQUIRE(a_steps == 2);
    REQUIRE(b_steps == 2);
    REQUIRE(c_steps == 2);
    REQUIRE(e_steps == 2);

    // Steps should be sorted by timestamp
    for (size_t i = 1; i < steps.size(); ++i) {
        REQUIRE(steps[i].timestamp_us >= steps[i - 1].timestamp_us);
    }
}

TEST_CASE("Signal2Step - Multiple samples with continuous movement", "[signal2step]") {
    using namespace sp::pipe;

    // A moves 1mm per sample for 5 samples
    std::vector<abce_pos_t> data = {
        { 0, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 2, 0, 0, 0 },
        { 3, 0, 0, 0 },
        { 4, 0, 0, 0 }
    };

    abce_pos_t mm_per_step = { 1, 1, 1, 1 };

    auto signal = make_source(data, 1000.0f) | type_erase_auto();

    std::vector<StepRecord> steps;
    signal2step::convert(
        std::move(signal),
        mm_per_step,
        [&](uint32_t timestamp_us, AxisEnum axis, bool direction) {
            steps.push_back({ timestamp_us, axis, direction });
        });

    // Should generate 4 steps (from 0 to 4)
    REQUIRE(steps.size() == 4);

    // Timestamps should span multiple sample intervals
    REQUIRE(steps[0].timestamp_us < 1000); // First interval
    REQUIRE(steps[1].timestamp_us >= 1000); // Second interval
    REQUIRE(steps[1].timestamp_us < 2000);
    REQUIRE(steps[2].timestamp_us >= 2000); // Third interval
    REQUIRE(steps[2].timestamp_us < 3000);
    REQUIRE(steps[3].timestamp_us >= 3000); // Fourth interval
}

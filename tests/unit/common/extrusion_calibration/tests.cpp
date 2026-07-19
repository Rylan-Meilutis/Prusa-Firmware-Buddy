#include <catch2/catch_test_macros.hpp>

#include <feature/extrusion_calibration.hpp>

using buddy::extrusion_calibration::Capture;

TEST_CASE("calibration rejects an empty or truncated capture") {
    Capture capture;
    capture.start();
    REQUIRE_FALSE(capture.score().valid);
    for (size_t i = 0; i < Capture::capacity + 1; ++i)
        capture.record(i * 3000, 0, 0);
    capture.stop();
    REQUIRE_FALSE(capture.score().valid);
}

TEST_CASE("calibration scores repeated extrusion transitions") {
    Capture capture;
    capture.start();
    float e = 0;
    for (size_t i = 0; i < 240; ++i) {
        const bool fast = (i / 40) % 2;
        e += fast ? 0.04f : 0.008f;
        const float load = (fast ? 20.0f : 4.0f) + ((i % 40) < 4 ? 3.0f : 0.0f);
        capture.record(i * 3000, load, e);
    }
    capture.stop();
    const auto score = capture.score();
    REQUIRE(score.valid);
    REQUIRE(score.transient > 0);
    REQUIRE(score.mean_load > 0);
}

TEST_CASE("job results are RAM-only and reset independently of anchors") {
    using namespace buddy::extrusion_calibration;
    set_job_result(2, { 0.04f, 15.0f, 0.8f, true });
    occupy_anchor(2);
    REQUIRE(job_result(2));
    reset_job_results();
    REQUIRE_FALSE(job_result(2));
    REQUIRE(occupied_anchor_mask() & (1u << 2));
    clear_anchor(2);
    REQUIRE_FALSE(occupied_anchor_mask() & (1u << 2));
}

TEST_CASE("runtime monitor detects missing pressure during executed E motion") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    for (uint32_t i = 1; i <= 220; ++i)
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
}

TEST_CASE("runtime monitor detects a sustained pressure collapse") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    uint32_t i = 1;
    for (; i <= 150; ++i)
        record_loadcell_sample(1'000 + i * 5'000, 25, i * 0.005f);
    for (; i <= 240; ++i)
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::pressure_collapse);
}

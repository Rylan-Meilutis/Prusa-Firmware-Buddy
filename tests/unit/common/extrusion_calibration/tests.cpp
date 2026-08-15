#include <catch2/catch_test_macros.hpp>

#include <feature/extrusion_calibration.hpp>

using buddy::extrusion_calibration::Capture;

TEST_CASE("calibration rejects an empty or truncated capture") {
    Capture capture;
    capture.start();
    REQUIRE_FALSE(capture.score().valid);
    for (size_t i = 0; i < Capture::capacity + 1; ++i) {
        capture.record(i * 3000, 0, 0);
    }
    capture.stop();
    REQUIRE_FALSE(capture.score().valid);
}

TEST_CASE("calibration capture excludes a paused cleaner wipe") {
    Capture capture;
    capture.start();
    capture.record(1'000, 2, 0.01f);
    capture.pause();
    capture.record(2'000, 100, 0.02f);
    capture.record(3'000, 100, 0.03f);
    REQUIRE(capture.size() == 1);
    capture.resume();
    capture.record(4'000, 3, 0.04f);
    capture.stop();
    REQUIRE(capture.size() == 2);
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

TEST_CASE("job results and anchor occupancy reset for a new print job") {
    using namespace buddy::extrusion_calibration;
    set_job_result(2, { 0.04f, 15.0f, 0.8f, true });
    occupy_anchor(2);
    REQUIRE(job_result(2));
    reset_job_results();
    REQUIRE_FALSE(job_result(2));
    REQUIRE_FALSE(occupied_anchor_mask() & (1u << 2));
}

TEST_CASE("runtime monitor detects missing pressure during executed E motion") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    for (uint32_t i = 1; i <= 1'800; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
}

TEST_CASE("runtime runout detection is fast but requires continuous meaningful extrusion") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);

    // Sub-threshold maintenance extrusion must not trip the quick path.
    for (uint32_t i = 1; i <= 600; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.001f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);

    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    for (uint32_t i = 1; i <= 300; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);

    // Start a fresh segment, then real forward motion with no pressure trips
    // in about two seconds rather than eight.
    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(3'006'000, 0, 0.6f);
    for (uint32_t i = 1; i <= 450; ++i) {
        record_loadcell_sample(3'006'000 + i * 5'000, 0, 0.6f + i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
}

TEST_CASE("runtime monitor detects a sustained pressure collapse") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    set_pressure_monitor_detection(false, true);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    uint32_t i = 1;
    for (; i <= 500; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 25, i * 0.005f);
    }
    for (; i <= 1'700; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::pressure_collapse);
}

TEST_CASE("runtime monitor ignores pressure shifts separated by layer travel") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    // Collapse monitoring must ignore layer-separated baseline shifts. The
    // optional fast runout policy intentionally treats a truly pressure-free
    // seven-second perimeter as runout, so it is tested separately above.
    set_pressure_monitor_detection(false, true);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    uint32_t sample = 1;
    float e = 0;
    record_loadcell_sample(sample++ * 5'000, 0, e);
    for (uint8_t layer = 0; layer < 4; ++layer) {
        // A long low-pressure perimeter that would have crossed the previous
        // 2 s qualification + 3 s fault threshold.
        for (uint16_t i = 0; i < 1'400; ++i) {
            e += 0.005f;
            record_loadcell_sample(sample++ * 5'000, 0, e);
        }
        for (uint16_t i = 0; i < 80; ++i) {
            record_loadcell_sample(sample++ * 5'000, 8.0f + layer, e);
        }
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
}

TEST_CASE("runtime monitor obeys independent presence and movement policies") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };

    reset_pressure_monitor();
    set_pressure_monitor_detection(false, true);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    for (uint32_t i = 1; i <= 1'800; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);

    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    uint32_t i = 1;
    for (; i <= 500; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 25, i * 0.005f);
    }
    for (; i <= 1'700; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
}

TEST_CASE("max-flow breakout remains active when optional filament policies are disabled") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    set_pressure_monitor_detection(false, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);

    uint32_t i = 1;
    // Establish a sustained pressure above the calibrated flow curve so the
    // existing soft max-flow marker is armed.
    for (; i <= 900; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 40, i * 0.01f);
    }
    // A subsequent sustained collapse promotes that marker to the original
    // always-on flow_breakout safety fault.
    for (; i <= 2'100; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.01f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::flow_breakout);
}

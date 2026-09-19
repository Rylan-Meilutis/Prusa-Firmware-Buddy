#include <catch2/catch_test_macros.hpp>

#include <feature/extrusion_calibration.hpp>
#include <pa_calibration_cache.hpp>
#include <pa_calibration_cache_storage.hpp>
#include <cstdlib>

TEST_CASE("PA flash records survive reopen and reject corruption and truncation") {
    using namespace buddy::pa_cache;
    char directory[] = "/tmp/rme-pa-cache-test-XXXXXX";
    REQUIRE(mkdtemp(directory) != nullptr);
    char path[128], temporary[128];
    snprintf(path, sizeof(path), "%s/cache.bin", directory);
    snprintf(temporary, sizeof(temporary), "%s/cache.tmp", directory);
    Record saved {};
    saved.version = record_version;
    saved.pa = 0.035f;
    saved.max_flow = 12;
    saved.confidence = 0.9f;
    saved.key.confidence_floor = 75;
    saved.high_load = 20;
    saved.noise = 0.2f;
    REQUIRE(write_record(path, temporary, saved));
    Record loaded {};
    REQUIRE(read_record(path, loaded));
    REQUIRE(loaded == saved);
    REQUIRE(loaded.matches(saved.key));
    int fd = open(path, O_WRONLY);
    REQUIRE(fd >= 0);
    REQUIRE(write(fd, "bad", 3) == 3);
    close(fd);
    REQUIRE_FALSE(read_record(path, loaded));
    REQUIRE(write_record(path, temporary, saved));
    fd = open(path, O_WRONLY | O_TRUNC);
    REQUIRE(fd >= 0);
    close(fd);
    REQUIRE_FALSE(read_record(path, loaded));
    unlink(path);
    REQUIRE_FALSE(read_record(path, loaded));
    rmdir(directory);
}

TEST_CASE("persistent PA records reject stale identities and invalid measurements") {
    using namespace buddy::pa_cache;
    Key key {};
    key.profile[0] = 'P';
    key.temperature = 220;
    key.nozzle = 0.4f;
    key.confidence_floor = 75;
    Record record { key, 0.03f, 12.f, 0.9f, 1.f, 20.f, 0.2f, record_version };
    REQUIRE(record.matches(key));
    auto legacy = record;
    legacy.version = 1;
    REQUIRE_FALSE(legacy.matches(key));
    auto flexible_key = key;
    flexible_key.flexible = true;
    REQUIRE_FALSE(record.matches(flexible_key));
    REQUIRE_FALSE(Record {}.matches(key));
    auto changed = key;
    changed.temperature = 235;
    REQUIRE_FALSE(record.matches(changed));
    changed = key;
    changed.profile[1] = 'X';
    REQUIRE_FALSE(record.matches(changed));
    changed = key;
    changed.color = 0xff;
    REQUIRE_FALSE(record.matches(changed));
    changed = key;
    changed.manufacturer = 2;
    REQUIRE_FALSE(record.matches(changed));
    changed = key;
    changed.physical_tool = 7;
    REQUIRE_FALSE(record.matches(changed));
    changed = key;
    changed.nozzle = 0.6f;
    REQUIRE_FALSE(record.matches(changed));
    record.confidence = 0.5f;
    REQUIRE_FALSE(record.matches(key));
    record.confidence = 0.9f;
    record.noise = NAN;
    REQUIRE_FALSE(record.matches(key));
    STATIC_REQUIRE(sizeof(Record) <= 128);
}

TEST_CASE("cached PA follows the selected logical filament rather than batch order") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    set_job_result(0, { 0.02f, 10, 0.9f, true });
    set_job_result(7, { 0.06f, 12, 0.9f, true });
    select_job_result(0);
    REQUIRE(calibrated_pressure_advance_or(0.1f) == 0.02f);
    select_job_result(7);
    REQUIRE(calibrated_pressure_advance_or(0.1f) == 0.06f);
    select_job_result(max_logical_filaments);
    REQUIRE(calibrated_pressure_advance_or(0.1f) == 0.1f);
    reset_job_results();
    REQUIRE(job_result(0) == nullptr);
}

using buddy::extrusion_calibration::Capture;

TEST_CASE("calibration capture does not permanently consume the sample buffer") {
    // Keeping the 15 KiB sample array inside the global Capture object starves
    // phase-stepping calibration even when M976 is idle.
    STATIC_REQUIRE(sizeof(Capture) < 64);
    Capture capture;
    REQUIRE(capture.start());
    capture.record(1'000, 2, 0.01f);
    capture.stop();
    REQUIRE(capture.size() == 1);
    capture.release();
    REQUIRE(capture.size() == 0);
    REQUIRE(capture.start());
}

TEST_CASE("calibration rejects an empty or truncated capture") {
    Capture capture;
    REQUIRE(capture.start());
    REQUIRE_FALSE(capture.score().valid);
    for (size_t i = 0; i < Capture::capacity + 1; ++i) {
        capture.record(i * 3000, 0, 0);
    }
    capture.stop();
    REQUIRE_FALSE(capture.score().valid);
}

TEST_CASE("calibration capture excludes a paused cleaner wipe") {
    Capture capture;
    REQUIRE(capture.start());
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
    REQUIRE(capture.start());
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

TEST_CASE("flexible PA transitions remain measurable at reduced feed speeds") {
    Capture capture;
    REQUIRE(capture.start());
    float e = 0;
    for (size_t i = 0; i < 240; ++i) {
        const bool fast = (i / 40) % 2;
        e += (fast ? 1.5f : 0.2f) * 0.003f;
        const float load = (fast ? 20.0f : 4.0f) + ((i % 40) < 4 ? 3.0f : 0.0f);
        capture.record(i * 3000, load, e);
    }
    capture.stop();
    REQUIRE(capture.score().valid);
}

TEST_CASE("selected flexible PA reference monitors slow extrusion") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    reset_pressure_monitor();
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    set_job_result(3, { 0.08f, 4.0f, 0.9f, true, reference, true });
    select_job_result(3);
    set_pressure_monitor_detection(true, false);
    record_loadcell_sample(1'000, 0, 0);
    for (uint32_t i = 1; i <= 1'800; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.0025f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
    reset_job_results();
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

TEST_CASE("runtime runout survives samples between discrete extruder steps") {
    using namespace buddy::extrusion_calibration;
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    // 200 Hz loadcell sampling, 100 Hz discrete E steps: every other
    // sample has no position change despite continuous 1 mm/s extrusion.
    for (uint32_t i = 1; i <= 1'800; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, (i / 2) * 0.01f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
}

TEST_CASE("runtime runout still resets across real travel and retraction") {
    using namespace buddy::extrusion_calibration;
    Score reference { .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    for (const bool retract : { false, true }) {
        reset_pressure_monitor();
        set_pressure_monitor_detection(true, false);
        configure_pressure_monitor(reference, 0.8f, 8.0f);
        uint32_t time = 1'000;
        float e = 0;
        record_loadcell_sample(time, 0, e);
        for (unsigned segment = 0; segment < 12; ++segment) {
            for (unsigned i = 0; i < 200; ++i) {
                e += 0.005f;
                record_loadcell_sample(time += 5'000, 0, e);
            }
            for (unsigned i = 0; i < 60; ++i) {
                if (retract) {
                    e -= 0.005f;
                }
                record_loadcell_sample(time += 5'000, 0, e);
            }
        }
        REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    }
}

TEST_CASE("quantized healthy extrusion is accepted and a later break is detected") {
    using namespace buddy::extrusion_calibration;
    Score reference { .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    reset_pressure_monitor();
    set_pressure_monitor_detection(true, true);
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    uint32_t i = 1;
    for (; i <= 1'000; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 25, (i / 2) * 0.01f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    for (; i <= 2'500; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, (i / 2) * 0.01f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::pressure_collapse);
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

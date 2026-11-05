#include <catch2/catch_test_macros.hpp>

#include <feature/extrusion_calibration.hpp>
#include <pa_calibration_cache.hpp>
#include <pa_calibration_cache_storage.hpp>
#include <cstdlib>
#include <cmath>
#include <m976_extrusion_policy.hpp>
#include <m976_search_policy.hpp>
#include <thread>
#include <atomic>

TEST_CASE("Capture motion gate excludes queue waits but retains stationary noise captures") {
    using namespace buddy::extrusion_calibration;
    auto &buffer = capture();
    REQUIRE(buffer.start());
    buffer.pause();
    buffer.resume_on_motion(5);
    for (unsigned i = 0; i < 10000; ++i) {
        buffer.record(i, 0, 5);
    }
    REQUIRE(buffer.size() == 0);
    for (unsigned i = 0; i < 100; ++i) {
        buffer.record(10000 + i, 1, 5 + .01f * (i + 1));
    }
    REQUIRE(buffer.stop() == 100);
    buffer.release();
    REQUIRE(buffer.start());
    for (unsigned i = 0; i < 100; ++i) {
        buffer.record(i, i % 2 ? 1 : -1, 0);
    }
    REQUIRE(buffer.stop() == 100);
    REQUIRE(buffer.noise_floor() > 0);
    buffer.release();
}

TEST_CASE("Capture producer cannot use released buffers across repeated jobs") {
    using namespace buddy::extrusion_calibration;
    auto &buffer = capture();
    std::atomic_bool running { true };
    std::thread producer([&] {
        unsigned time = 0;
        while (running.load()) {
            ++time;
            buffer.record(time, 1, time * .01f);
        }
    });
    for (int attempt = 0; attempt < 100; ++attempt) {
        REQUIRE(buffer.start());
        buffer.pause();
        buffer.resume_on_motion(0);
        buffer.stop();
        buffer.release();
    }
    running.store(false);
    producer.join();
    REQUIRE(buffer.size() == 0);
}

TEST_CASE("Retained extrusion limits preserve capture time and monitor reference units") {
    using namespace buddy::m976_extrusion_policy;
    for (const float cap : { 2.f, 4.f, 5.f, 8.f, 100.f }) {
        const auto bounded = bounded_speeds(false, cap);
        REQUIRE(bounded.high_mm_s <= cap);
        REQUIRE(bounded.high_mm_s > bounded.low_mm_s);
        buddy::extrusion_calibration::Score measured;
        measured.valid = true;
        measured.low_load = 3 + 10 * bounded.low_mm_s;
        measured.high_load = 3 + 10 * bounded.high_mm_s;
        measured.noise = .25f;
        const auto reference = buddy::extrusion_calibration::nominal_pressure_reference(measured, bounded.low_mm_s, bounded.high_mm_s, false);
        REQUIRE(reference.valid);
        REQUIRE(reference.low_load == 11.f);
        REQUIRE(std::abs(reference.high_load - 83.f) < .0001f);
        REQUIRE(reference.noise == .25f);
    }
    REQUIRE(bounded_speeds(false, 0).high_mm_s == 0);
    REQUIRE(bounded_speeds(false, NAN).high_mm_s == 0);
    REQUIRE(bounded_speeds(true, 100).high_mm_s == 1.f);
}

TEST_CASE("Temperature PA regression is bounded and single-point jobs stay single") {
    using namespace buddy::pa_temperature;
    Model model;
    REQUIRE_FALSE(model.evaluate(240).valid);
    REQUIRE(model.add({ 250, .040f, 12, .9f, 1, 20, .2f, true, false }));
    REQUIRE(model.count == 1);
    REQUIRE(model.evaluate(250).valid);
    REQUIRE_FALSE(model.evaluate(240).valid);
    REQUIRE(model.add({ 240, .050f, 11, .8f, 2, 22, .3f, true, false }));
    REQUIRE(model.count == 2);
    auto result = model.evaluate(245);
    REQUIRE(result.valid);
    REQUIRE(std::abs(result.point.pa - .045f) < .00001f);
    REQUIRE(std::abs(result.slope + .001f) < .00001f);
    REQUIRE(result.point.max_flow == 11);
    REQUIRE(result.point.noise == .3f);
    REQUIRE_FALSE(model.evaluate(120).valid);
    REQUIRE_FALSE(model.evaluate(251).valid);
    REQUIRE(model.add({ 240, .050f, 11, .8f, 2, 22, .3f, true, false }));
    REQUIRE(model.count == 2);
    REQUIRE(model.add({ 260, .030f, 13, .9f, 1, 20, .2f, true, false }));
    REQUIRE(std::abs(model.evaluate(255).point.pa - .035f) < .00001f);
    REQUIRE(model.add({ 250, .15f, 12, .9f, 1, 20, .2f, true, false }));
    REQUIRE(std::string_view(model.evaluate(245).method) == "piecewise");
    REQUIRE(model.evaluate(250).point.pa == .15f); // Measured values are authoritative.
    REQUIRE_FALSE(model.add({ 100, .04f, 12, .9f }));
    REQUIRE_FALSE(model.add({ 270, NAN, 12, .9f }));
    REQUIRE_FALSE(model.add({ 270, .04f, 12, 0 }));
}

TEST_CASE("HIPS temperature cache preserves both 250 and 240 across reopening") {
    using namespace buddy::pa_cache;
    char directory[] = "/tmp/rme-pa-bank-test-XXXXXX";
    REQUIRE(mkdtemp(directory) != nullptr);
    char path[128], temporary[128];
    snprintf(path, sizeof(path), "%s/cache.bin", directory);
    snprintf(temporary, sizeof(temporary), "%s/cache.tmp", directory);
    Record saved {};
    saved.version = record_version;
    saved.key.temperature = 250;
    saved.key.confidence_floor = 75;
    saved.pa = .04f;
    saved.max_flow = 12;
    saved.confidence = .9f;
    saved.high_load = 20;
    saved.noise = .2f;
    REQUIRE(write_record(path, temporary, saved));
    Bank bank;
    REQUIRE(read_bank(path, bank)); // Migrate the previous single-point file.
    REQUIRE(bank.count == 1);
    auto lower = saved;
    lower.key.temperature = 240;
    lower.pa = .05f;
    REQUIRE(bank.insert(lower));
    REQUIRE(write_bank(path, temporary, bank));
    for (int job = 0; job < 4; ++job) {
        buddy::extrusion_calibration::reset_job_results();
        Bank reopened;
        REQUIRE(read_bank(path, reopened));
        REQUIRE(reopened == bank);
        const char *reason = nullptr;
        REQUIRE(reopened.find(saved.key, reason)->pa == .04f);
        REQUIRE(reason == nullptr);
        REQUIRE(reopened.find(lower.key, reason)->pa == .05f);
        auto changed = lower.key;
        changed.nozzle = .6f;
        REQUIRE(reopened.find(changed, reason) == nullptr);
        REQUIRE(std::string_view(reason) == "nozzle_changed");
    }
    REQUIRE_FALSE(write_bank(path, "/nonexistent-pa-cache-directory/cache.tmp", bank));
    Bank reopened;
    REQUIRE(read_bank(path, reopened));
    REQUIRE(reopened == bank);
    for (int t = 251; t < 262; ++t) {
        auto extra = saved;
        extra.key.temperature = t;
        REQUIRE(bank.insert(extra));
    }
    REQUIRE(bank.count == buddy::pa_temperature::capacity);
    auto changed = lower;
    changed.key.nozzle = .6f;
    REQUIRE(bank.insert(changed));
    REQUIRE(bank.count == 1);
    int fd = open(path, O_WRONLY);
    REQUIRE(fd >= 0);
    REQUIRE(write(fd, "bad", 3) == 3);
    close(fd);
    REQUIRE_FALSE(read_bank(path, reopened));
    unlink(path);
    rmdir(directory);
}

TEST_CASE("Job PA changes with print target but ignores maintenance temperatures") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    Result result { .04f, 12, .9f, true, {}, false, 250 };
    result.pressure_reference.valid = true;
    result.pressure_reference.low_load = 1;
    result.pressure_reference.high_load = 20;
    result.pressure_reference.noise = .2f;
    set_job_result(0, result);
    result.pressure_advance = .05f;
    result.calibration_temperature = 240;
    set_job_result(0, result);
    select_job_result(0);
    REQUIRE(std::abs(calibrated_pressure_advance_or(0) - .04f) < .00001f);
    REQUIRE(request_job_temperature(0, 240));
    select_job_result(0);
    REQUIRE(std::abs(calibrated_pressure_advance_or(0) - .05f) < .00001f);
    REQUIRE(request_job_temperature(0, 245));
    REQUIRE_FALSE(has_measured_job_temperature(0, 245));
    REQUIRE(has_measured_job_temperature(0, 240));
    select_job_result(0);
    REQUIRE(std::abs(calibrated_pressure_advance_or(0) - .045f) < .00001f);
    REQUIRE_FALSE(request_job_temperature(0, 120));
    REQUIRE(job_result(0)->calibration_temperature == 245);
    REQUIRE_FALSE(request_job_temperature(1, 245));
    Result fallback { .035f, 10, .5f, true };
    set_job_result(0, fallback);
    select_job_result(0);
    REQUIRE(temperature_model(0) == nullptr);
    REQUIRE(calibrated_pressure_advance_or(0) == .035f);
    reset_job_results();
    REQUIRE(temperature_model(0) == nullptr);
    REQUIRE_FALSE(request_job_temperature(0, 240));
}

TEST_CASE("Each sheet-cleanup temperature measurement reserves a fresh anchor") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    REQUIRE(reserve_measurement_anchor(0) == 0);
    REQUIRE(reserve_measurement_anchor(0) == 1);
    REQUIRE(reserve_measurement_anchor(1) == 2);
    for (uint8_t slot = 3; slot < 8; ++slot) {
        REQUIRE(reserve_measurement_anchor(0) == slot);
    }
    REQUIRE_FALSE(reserve_measurement_anchor(0).has_value());
    REQUIRE(occupied_anchor_mask() == 255);
    clear_anchor(3);
    REQUIRE(reserve_measurement_anchor(0) == 3);
    REQUIRE_FALSE(reserve_measurement_anchor(0).has_value());
    reset_job_results();
    REQUIRE(occupied_anchor_mask() == 0);
    REQUIRE(reserve_measurement_anchor(7) == 7);
    REQUIRE(reserve_measurement_anchor(7) == 0);
    reset_job_results();
}

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
    // A failed replacement must preserve the last successful calibration.
    auto replacement = saved;
    replacement.pa = 0.08f;
    char unavailable[160];
    snprintf(unavailable, sizeof(unavailable), "%s/missing/cache.tmp", directory);
    REQUIRE_FALSE(write_record(path, unavailable, replacement));
    REQUIRE(read_record(path, loaded));
    REQUIRE(loaded == saved);
    // New print jobs clear RAM results, but must not affect the flash record.
    for (int print = 0; print < 3; ++print) {
        buddy::extrusion_calibration::reset_job_results();
        loaded = {};
        REQUIRE(read_record(path, loaded));
        REQUIRE(loaded.matches(saved.key));
    }
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

TEST_CASE("maintenance pressure suspension survives nesting and early exits then rearms") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
    set_job_result(0, { 0.046f, 12.f, 0.81f, true, reference, false, 250 });
    select_job_result(0);
    set_pressure_monitor_detection(true, true);
    uint32_t time = 1000;
    float e = 0;
    const auto move_without_pressure = [&] {
        for (int i = 0; i < 2000; ++i) {
            time += 5000;
            e += 0.005f;
            record_loadcell_sample(time, 0, e);
        }
    };
    const auto calibrate = [&] {
        PressureMonitorSuspension outer;
        {
            PressureMonitorSuspension nested;
            select_job_result(0); // Tool selection must not release the outer hold.
            move_without_pressure();
            REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
        }
        move_without_pressure();
        REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
        return false; // Simulate G427 failure/abort before normal completion.
    };
    REQUIRE_FALSE(calibrate());
    REQUIRE(job_result(0)->calibration_temperature == 250);
    move_without_pressure();
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
    reset_job_results();
    REQUIRE(job_result(0) == nullptr);
}

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

TEST_CASE("rigid PA scoring does not mistake quantized E steps for pressure transitions") {
    // Four native 250 ms high / 500 ms low cycles sampled at 320 Hz.
    // A sharp, clean PLA pressure response must not acquire dozens of fake
    // transitions merely because executed E is reported in physical steps.
    for (const float step_mm : { .0025f, .005f, .01f }) {
        Capture buffer;
        REQUIRE(buffer.start());
        float e = 0;
        for (unsigned i = 0; i < 960; ++i) {
            const bool fast = i % 240 < 80;
            e += (fast ? 8.f : .8f) / 320.f;
            buffer.record(i * 3125, fast ? 500.f : 50.f, std::round(e / step_mm) * step_mm);
        }
        buffer.stop();
        const auto score = buffer.score();
        CAPTURE(step_mm, score.transitions_detected, score.transitions_used, score.rejected_low_signal);
        REQUIRE(score.valid);
        REQUIRE(score.transitions_detected <= 8);
        REQUIRE(score.rejected_low_signal == 0);
        REQUIRE(score.transient_stddev < .1f);
    }
}

TEST_CASE("PA repeat evidence pools costs and preserves between-capture disagreement") {
    using buddy::extrusion_calibration::Score;
    using buddy::m976_search::combine_repeated_scores;
    Score a { .transient = .1f, .mean_load = 100, .noise = 1, .low_load = 20, .high_load = 120, .valid = true, .sample_count = 900, .transitions_detected = 8, .transitions_used = 8, .transient_stddev = .02f };
    auto b = a;
    b.transient = .3f;
    b.noise = 2;
    const auto pooled = combine_repeated_scores(a, b);
    REQUIRE(pooled.valid);
    REQUIRE(std::abs(pooled.transient - .2f) < .00001f);
    REQUIRE(std::abs(pooled.transient_stddev - std::sqrt(.0104f)) < .00001f);
    REQUIRE(pooled.transitions_used == 16);
    REQUIRE(pooled.sample_count == 1800);
    REQUIRE(pooled.noise == 2);
    b.valid = false;
    REQUIRE_FALSE(combine_repeated_scores(a, b).valid);
    b = a;
    b.capture_overflow = true;
    REQUIRE_FALSE(combine_repeated_scores(a, b).valid);
    auto consistent = a;
    for (unsigned repeat = 0; repeat < 10000; ++repeat) {
        consistent = combine_repeated_scores(consistent, a);
    }
    REQUIRE(consistent.sample_count == 65535);
    REQUIRE(consistent.transitions_used == 65535);
    REQUIRE(std::abs(consistent.transient_stddev - .02f) < .0001f);
}

TEST_CASE("PA scorer never joins pressure plateaus across a paused pellet cleanup") {
    buddy::extrusion_calibration::Capture buffer;
    REQUIRE(buffer.start());
    float e = 0;
    for (unsigned i = 0; i < 960; ++i) {
        const bool fast = i % 240 < 80;
        e += (fast ? 8.f : .8f) / 320.f;
        // Cooling samples are excluded from capture, leaving real time gaps.
        buffer.record(i * 3125 + (i / 240) * 15000000, fast ? 500.f : 50.f, std::round(e / .005f) * .005f);
    }
    buffer.stop();
    const auto score = buffer.score();
    CAPTURE(score.transitions_detected, score.transitions_used, score.rejected_low_signal);
    REQUIRE(score.valid);
    REQUIRE(score.transitions_used == 4); // Only the four uninterrupted fast->slow edges.
}

TEST_CASE("windowed PA scoring preserves transient ordering for rigid and flexible materials") {
    for (const bool flexible : { false, true }) {
        for (const float amplitude : { 25.f, 450.f }) {
            for (const float sign : { -1.f, 1.f }) {
                const auto measure = [&](float overshoot) {
                    buddy::extrusion_calibration::Capture buffer;
                    REQUIRE(buffer.start());
                    float e = 0;
                    const auto speeds = buddy::m976_extrusion_policy::speeds(flexible);
                    for (unsigned i = 0; i < 960; ++i) {
                        const unsigned phase = i % 240;
                        const bool fast = phase < 80;
                        e += (fast ? speeds.high_mm_s : speeds.low_mm_s) / 320.f;
                        const float transient = phase < 12 ? overshoot : phase >= 80 && phase < 92 ? -overshoot
                                                                                                   : 0;
                        const float noise = i % 2 ? .1f : -.1f;
                        buffer.record(i * 3125, sign * (10 + amplitude * (float(fast) + transient)) + noise,
                            std::round(e / .005f) * .005f);
                    }
                    buffer.stop();
                    return buffer.score(flexible);
                };
                const auto settled = measure(0), oscillating = measure(.5f);
                CAPTURE(flexible, amplitude, sign);
                REQUIRE(settled.valid);
                REQUIRE(oscillating.valid);
                REQUIRE(settled.transitions_used <= 8);
                REQUIRE(oscillating.transient > settled.transient);
            }
        }
    }
}

TEST_CASE("windowed PA scoring still rejects missing pressure and steady quantized feed") {
    for (const bool pressure_changes : { false, true }) {
        buddy::extrusion_calibration::Capture buffer;
        REQUIRE(buffer.start());
        float e = 0;
        for (unsigned i = 0; i < 960; ++i) {
            const bool fast = i % 240 < 80;
            e += (pressure_changes ? .8f : fast ? 8.f
                                                : .8f)
                / 320.f;
            const float load = pressure_changes ? (fast ? 500.f : 50.f) : 10.f;
            buffer.record(i * 3125, load, std::round(e / .005f) * .005f);
        }
        buffer.stop();
        REQUIRE_FALSE(buffer.score().valid);
    }
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
    for (uint32_t i = 1; i <= 3'600; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.0025f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::no_pressure_rise);
    reset_job_results();
}

TEST_CASE("flexible pressure buildup does not trigger the rigid runout window") {
    using namespace buddy::extrusion_calibration;
    for (const bool recovery : { false, true }) {
        reset_job_results();
        Score reference { .transient = 0.2f, .mean_load = 25, .noise = 0.2f, .low_load = 5, .high_load = 30, .valid = true };
        set_job_result(3, { 0.08f, 4.0f, 0.9f, true, reference, true });
        select_job_result(3);
        set_pressure_monitor_detection(true, true);
        if (recovery) {
            suspend_pressure_monitor(true);
            suspend_pressure_monitor(false);
        }
        record_loadcell_sample(1'000, 0, 0);
        // Five seconds / 2.5 mm of elastic take-up, then healthy pressure.
        for (uint32_t i = 1; i <= 3'600; ++i) {
            record_loadcell_sample(1'000 + i * 5'000, i <= 1'000 ? 0 : 15, i * 0.0025f);
            REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
        }
        reset_job_results();
    }
}

TEST_CASE("primed pressure and retained travel pressure cannot become idle zero") {
    using namespace buddy::extrusion_calibration;
    for (const bool flexible : { false, true }) {
        for (const float sign : { -1.f, 1.f }) {
            for (const float zero : { -40.f, 0.f, 40.f }) {
                for (const bool travel : { false, true }) {
                    reset_job_results();
                    const float low_speed = flexible ? .2f : .8f;
                    const float high_speed = flexible ? 1.f : 8.f;
                    Score reference { .noise = .2f, .low_load = zero + sign * 5, .high_load = zero + sign * 30, .valid = true };
                    configure_pressure_monitor(reference, low_speed, high_speed, flexible);
                    set_pressure_monitor_detection(true, true);
                    uint32_t time = 1000;
                    float e = 0;
                    const float healthy = zero + sign * 15;
                    // Already primed at the FIRST runtime sample, before any
                    // object move. This previously became the idle baseline.
                    record_loadcell_sample(time, healthy, e);
                    for (int segment = 0; segment < (travel ? 2 : 1); ++segment) {
                        for (unsigned i = 0; i < 3600; ++i) {
                            e += flexible ? .0025f : .005f;
                            record_loadcell_sample(time += 5000, healthy, e);
                            REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
                        }
                        if (travel && !segment) {
                            for (unsigned i = 0; i < 200; ++i) {
                                record_loadcell_sample(time += 5000, healthy, e);
                            }
                        }
                    }
                    // Existing pressure is evidence, not permission to ignore
                    // a subsequent real loss. Both material policies still
                    // detect sustained collapse after continuous feeding.
                    ExtrusionFault fault = ExtrusionFault::none;
                    for (unsigned i = 0; i < 7200 && fault == ExtrusionFault::none; ++i) {
                        e += flexible ? .0025f : .005f;
                        record_loadcell_sample(time += 5000, zero, e);
                        fault = consume_extrusion_fault();
                    }
                    REQUIRE(fault == ExtrusionFault::pressure_collapse);
                }
            }
        }
    }
    reset_job_results();
}

TEST_CASE("pre-print INDX bin purge is excluded but first object extrusion rearms") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    Score reference { .noise = .2f, .low_load = 5, .high_load = 30, .valid = true };
    set_job_result(4, { .08f, 4.f, .9f, true, reference, true });
    select_job_result(4);
    set_pressure_monitor_detection(true, true);
    uint32_t time = 1000;
    float e = 0;
    set_pressure_monitor_purge_context(true); // Successful G12 S90
    set_pressure_monitor_purge_context(true); // Idempotent, not a counted hold
    select_job_result(4); // Repeated T4 must not release the cleaner scope.
    for (unsigned i = 0; i < 10000; ++i) {
        e += .0025f;
        record_loadcell_sample(time += 5000, 0, e);
        REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    }
    REQUIRE(pressure_monitor_purge_context());
    set_pressure_monitor_purge_context(false); // S91 or executed XY leaves bin
    REQUIRE_FALSE(pressure_monitor_purge_context());
    for (unsigned i = 0; i < 3600; ++i) {
        e += .0025f;
        record_loadcell_sample(time += 5000, 15, e);
        REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    }
    // A genuinely empty first object segment still raises runout after a
    // fresh selection; no lingering purge hold or weakened policy.
    select_job_result(4);
    ExtrusionFault fault = ExtrusionFault::none;
    for (unsigned i = 0; i < 7200 && fault == ExtrusionFault::none; ++i) {
        e += .0025f;
        record_loadcell_sample(time += 5000, 0, e);
        fault = consume_extrusion_fault();
    }
    REQUIRE(fault == ExtrusionFault::no_pressure_rise);
    set_pressure_monitor_purge_context(true); // Cancel before S91
    reset_job_results();
    REQUIRE_FALSE(pressure_monitor_purge_context());
}

TEST_CASE("flexible true empty feed remains detectable across reference slopes and feed rates") {
    using namespace buddy::extrusion_calibration;
    for (const float high : { 15.f, 30.f, 55.f }) {
        for (const float velocity : { .2f, .3f, .5f, 1.f }) {
            reset_job_results();
            Score reference { .noise = .2f, .low_load = 5, .high_load = high, .valid = true };
            configure_pressure_monitor(reference, .2f, 1.f, true);
            set_pressure_monitor_detection(true, true);
            record_loadcell_sample(1000, 0, 0);
            ExtrusionFault fault = ExtrusionFault::none;
            for (unsigned i = 1; i <= 18000 && fault == ExtrusionFault::none; ++i) {
                record_loadcell_sample(1000 + i * 5000, 0, i * velocity * .005f);
                fault = consume_extrusion_fault();
            }
            INFO("high=" << high << " velocity=" << velocity);
            REQUIRE(fault == ExtrusionFault::no_pressure_rise);
        }
    }
    reset_job_results();
}

TEST_CASE("flexible calibration scores quantized slow E steps without false edges") {
    for (const bool pulsed : { false, true }) {
        Capture capture;
        REQUIRE(capture.start());
        float e = 0;
        for (size_t i = 0; i < 960; ++i) {
            const bool fast = pulsed && (i % 240 < 80);
            e += (fast ? 1.0f : 0.2f) / 320.0f;
            capture.record(i * 3125, fast ? 20.0f : 4.0f, std::floor(e / 0.002f) * 0.002f);
        }
        capture.stop();
        const auto score = capture.score(true);
        REQUIRE(score.valid == pulsed);
        if (pulsed) {
            REQUIRE(score.transitions_used >= 4);
            REQUIRE(score.transitions_detected <= 8);
        }
    }
}

TEST_CASE("fallback PA without a trusted reference reports unavailable protection, not runout") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    reset_pressure_monitor();
    set_job_result(4, { 0.08f, 2.4f, 0.2f, true, {}, true });
    select_job_result(4);
    set_pressure_monitor_detection(true, true);
    for (uint32_t i = 0; i < 2000; ++i) {
        record_loadcell_sample(1000 + i * 5000, 0, i * 0.005f);
    }
    REQUIRE_FALSE(pressure_monitor_has_reference());
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::reference_unavailable);
    acknowledge_extrusion_fault();
    set_pressure_monitor_detection(false, false);
    for (uint32_t i = 0; i < 2000; ++i) {
        record_loadcell_sample(20'000'000 + i * 5000, 0, 10 + i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    reset_job_results();
}

TEST_CASE("unavailable pressure reference respects purge and maintenance guards on tool eight") {
    using namespace buddy::extrusion_calibration;
    reset_job_results();
    set_job_result(7, { .04f, 2.4f, .2f, true });
    select_job_result(7);
    set_pressure_monitor_detection(true, true);
    uint32_t time = 1000;
    float e = 0;
    const auto feed = [&] {
        for (unsigned i = 0; i < 2000; ++i) {
            time += 5000;
            e += .005f;
            record_loadcell_sample(time, 0, e);
        }
    };
    set_pressure_monitor_purge_context(true);
    feed();
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    set_pressure_monitor_purge_context(false);
    {
        PressureMonitorSuspension maintenance;
        feed();
        REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
    }
    feed();
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::reference_unavailable);
    reset_job_results();
    feed();
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);
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
    REQUIRE_FALSE(pressure_monitor_presence_enabled());
    REQUIRE(pressure_monitor_movement_enabled());
    configure_pressure_monitor(reference, 0.8f, 8.0f);
    record_loadcell_sample(1'000, 0, 0);
    for (uint32_t i = 1; i <= 1'800; ++i) {
        record_loadcell_sample(1'000 + i * 5'000, 0, i * 0.005f);
    }
    REQUIRE(consume_extrusion_fault() == ExtrusionFault::none);

    reset_pressure_monitor();
    set_pressure_monitor_detection(true, false);
    REQUIRE(pressure_monitor_presence_enabled());
    REQUIRE_FALSE(pressure_monitor_movement_enabled());
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

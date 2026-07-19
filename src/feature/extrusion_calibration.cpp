#include "extrusion_calibration.hpp"

#include <algorithm>
#include <cmath>

namespace buddy::extrusion_calibration {

namespace {
Capture instance;
std::array<Result, max_logical_filaments> results {};
std::atomic_uint8_t anchors { 0 };
float profile_pressure_advance = NAN;
}

void Capture::start() {
    count_.store(0, std::memory_order_relaxed);
    active_.store(true, std::memory_order_release);
}

size_t Capture::stop() {
    active_.store(false, std::memory_order_release);
    return size();
}

void Capture::record(const uint32_t time_us, const float load_g, const float e_position_mm) {
    if (!active_.load(std::memory_order_acquire)) {
        return;
    }
    const size_t index = count_.fetch_add(1, std::memory_order_acq_rel);
    if (index < samples_.size()) {
        samples_[index] = { time_us, load_g, e_position_mm };
    } else {
        count_.store(samples_.size(), std::memory_order_release);
        active_.store(false, std::memory_order_release);
    }
}

Score Capture::score() const {
    const size_t n = std::min(size(), samples_.size());
    if (n < 32) {
        return {};
    }

    // Compare each load sample with a short future plateau. This directly
    // penalizes rise/fall overshoot and settling without depending on command
    // transport timestamps. E-position deltas identify the actual transitions.
    float cost = 0, load_sum = 0, noise_sum = 0;
    size_t used = 0;
    constexpr size_t lookahead = 12;
    for (size_t i = 2; i + lookahead < n; ++i) {
        const float dt0 = static_cast<float>(samples_[i].time_us - samples_[i - 1].time_us) * 1e-6f;
        const float dt1 = static_cast<float>(samples_[i - 1].time_us - samples_[i - 2].time_us) * 1e-6f;
        if (dt0 <= 0 || dt1 <= 0) {
            continue;
        }
        const float v0 = (samples_[i].e_position_mm - samples_[i - 1].e_position_mm) / dt0;
        const float v1 = (samples_[i - 1].e_position_mm - samples_[i - 2].e_position_mm) / dt1;
        if (std::abs(v0 - v1) < 0.25f) {
            noise_sum += std::abs(samples_[i].load_g - samples_[i - 1].load_g);
            continue;
        }
        float plateau = 0;
        for (size_t j = i + lookahead / 2; j < i + lookahead; ++j) {
            plateau += samples_[j].load_g;
        }
        plateau /= lookahead / 2;
        const float error = samples_[i].load_g - plateau;
        cost += error * error;
        load_sum += std::abs(plateau);
        ++used;
    }

    Score result;
    result.valid = used >= 3 && size() < samples_.size();
    if (result.valid) {
        result.transient = cost / used;
        result.mean_load = load_sum / used;
        result.noise = noise_sum / n;
    }
    return result;
}

Capture &capture() { return instance; }

void record_loadcell_sample(const uint32_t time_us, const float load_g, const float e_position_mm) {
    instance.record(time_us, load_g, e_position_mm);
}

void reset_job_results() {
    results = {};
    profile_pressure_advance = NAN;
    // Anchor occupancy intentionally survives job boundaries. Physical debris
    // does not disappear when a print ends.
}

void note_profile_pressure_advance(const float value) { profile_pressure_advance = value; }

float profile_pressure_advance_or(const float fallback) {
    return std::isfinite(profile_pressure_advance) ? profile_pressure_advance : fallback;
}

const Result *job_result(const size_t logical_filament) {
    return logical_filament < results.size() && results[logical_filament].valid ? &results[logical_filament] : nullptr;
}

void set_job_result(const size_t logical_filament, const Result &result) {
    if (logical_filament < results.size()) results[logical_filament] = result;
}

uint8_t occupied_anchor_mask() { return anchors.load(std::memory_order_acquire); }

void occupy_anchor(const size_t logical_filament) {
    if (logical_filament < max_logical_filaments)
        anchors.fetch_or(static_cast<uint8_t>(1u << logical_filament), std::memory_order_acq_rel);
}

void clear_anchor(const size_t logical_filament) {
    if (logical_filament < max_logical_filaments)
        anchors.fetch_and(static_cast<uint8_t>(~(1u << logical_filament)), std::memory_order_acq_rel);
}

} // namespace buddy::extrusion_calibration

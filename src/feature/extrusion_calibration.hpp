#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace buddy::extrusion_calibration {

struct Sample {
    uint32_t time_us;
    float load_g;
    float e_position_mm;
};

struct Score {
    float transient = std::numeric_limits<float>::infinity();
    float mean_load = 0;
    float noise = std::numeric_limits<float>::infinity();
    bool valid = false;
};

class Capture {
public:
    static constexpr size_t capacity = 2048;

    void start();
    size_t stop();
    void record(uint32_t time_us, float load_g, float e_position_mm);
    Score score() const;
    size_t size() const { return count_.load(std::memory_order_acquire); }

private:
    std::array<Sample, capacity> samples_ {};
    std::atomic_size_t count_ { 0 };
    std::atomic_bool active_ { false };
};

Capture &capture();
void record_loadcell_sample(uint32_t time_us, float load_g, float e_position_mm);

struct Result {
    float pressure_advance = 0;
    float max_flow_mm3_s = 0;
    float confidence = 0;
    bool valid = false;
};

static constexpr size_t max_logical_filaments = 6;
void reset_job_results();
const Result *job_result(size_t logical_filament);
void set_job_result(size_t logical_filament, const Result &result);
uint8_t occupied_anchor_mask();
void occupy_anchor(size_t logical_filament);
void clear_anchor(size_t logical_filament);
void note_profile_pressure_advance(float value);
float profile_pressure_advance_or(float fallback);

} // namespace buddy::extrusion_calibration

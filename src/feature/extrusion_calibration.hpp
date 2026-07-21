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
    float low_load = 0;
    float high_load = 0;
    bool valid = false;
    uint16_t sample_count = 0;
    uint16_t transitions_detected = 0;
    uint16_t transitions_used = 0;
    uint16_t rejected_low_signal = 0;
    uint16_t rejected_timing = 0;
    bool capture_overflow = false;
    float strongest_transition = 0;
    float highest_transition_noise = 0;
    float transient_stddev = std::numeric_limits<float>::infinity();
};

class Capture {
public:
    static constexpr size_t capacity = 2048;

    void start();
    size_t stop();
    void record(uint32_t time_us, float load_g, float e_position_mm);
    Score score() const;
    float noise_floor() const;
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
    Score pressure_reference {};
};

// Eight entries cover the full INDX tool complement while still fitting the
// uint8_t job/tool masks used by the calibration UI and progress FSM.
static constexpr size_t max_logical_filaments = 8;
void reset_job_results();
const Result *job_result(size_t logical_filament);
void set_job_result(size_t logical_filament, const Result &result);
float calibrated_pressure_advance_or(float fallback);
void set_calibration_command_active(bool active);
bool calibration_command_active();
uint8_t occupied_anchor_mask();
void occupy_anchor(size_t logical_filament);
void clear_anchor(size_t logical_filament);
void note_profile_pressure_advance(float value);
float profile_pressure_advance_or(float fallback);

enum class ExtrusionFault : uint8_t {
    none,
    no_pressure_rise,
    pressure_collapse,
    flow_breakout,
};

/// Arms runtime extrusion-health monitoring from a known-good PA response.
/// All force values are tared loadcell grams; velocities are filament mm/s.
void configure_pressure_monitor(const Score &reference, float low_velocity_mm_s, float high_velocity_mm_s);
void reset_pressure_monitor();
void suspend_pressure_monitor(bool suspend);
ExtrusionFault consume_extrusion_fault();
void acknowledge_extrusion_fault();

} // namespace buddy::extrusion_calibration

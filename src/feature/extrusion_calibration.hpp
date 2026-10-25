#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <pa_temperature_model.hpp>

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
    // Four bounded-speed plateaus capture about 1,000 samples. Pre-motion
    // planner waits are excluded by resume_on_motion(), leaving margin for
    // acceleration without permanently reserving the
    // former 24 KiB buffer from the system heap for a one-second operation.
    // score() already rejects capture_overflow, so an unexpectedly long run
    // remains fail-safe rather than consuming unbounded memory.
    static constexpr size_t capacity = 1280;

    /// Allocate the capture buffer and begin a new capture. The buffer exists
    /// only while M976 is measuring, leaving calibration/phase-stepping heap
    /// headroom untouched during normal operation.
    bool start();
    void pause();
    void resume();
    /// Exclude queue-start latency, then retain every sample once E moves.
    void resume_on_motion(float baseline_e_position_mm);
    /// Bound a forward-only burst to executed motion, not planner wake-up time.
    void resume_until_position(float baseline_e_position_mm, float end_e_position_mm);
    size_t stop();
    void release();
    void record(uint32_t time_us, float load_g, float e_position_mm);
    Score score(bool flexible = false) const;
    float noise_floor() const;
    size_t size() const { return count_.load(std::memory_order_acquire); }

private:
    std::unique_ptr<Sample[]> samples_;
    std::atomic_size_t count_ { 0 };
    std::atomic_bool active_ { false };
    // Protected together with the buffer by InterruptDisabler. This also
    // prevents a loadcell producer from using the buffer while it is released.
    bool waiting_for_motion_ = false;
    float motion_baseline_e_ = 0;
    float motion_end_e_ = std::numeric_limits<float>::infinity();
};

Capture &capture();
Score nominal_pressure_reference(const Score &measured, float low_mm_s, float high_mm_s, bool flexible);
void record_loadcell_sample(uint32_t time_us, float load_g, float e_position_mm);

struct Result {
    float pressure_advance = 0;
    float max_flow_mm3_s = 0;
    float confidence = 0;
    bool valid = false;
    Score pressure_reference {};
    bool flexible = false;
    int16_t calibration_temperature = 0; // Actual accepted request, not the restored idle target.
};

// Eight entries cover the full INDX tool complement while still fitting the
// uint8_t job/tool masks used by the calibration UI and progress FSM.
static constexpr size_t max_logical_filaments = 8;
void reset_job_results();
const Result *job_result(size_t logical_filament);
void set_job_result(size_t logical_filament, const Result &result);
void select_job_result(size_t logical_filament);
const pa_temperature::Model *temperature_model(size_t logical_filament);
bool request_job_temperature(size_t logical_filament, int16_t temperature);
bool has_measured_job_temperature(size_t logical_filament, int16_t temperature);
// Foreground, parser-facing temperature changes only (not maintenance heating).
void apply_print_temperature(uint8_t physical_tool, int16_t temperature);
// INDX hardware adapter: apply the selected tool's job result without motion.
void apply_selected_indx_result();
float calibrated_pressure_advance_or(float fallback);
void set_calibration_command_active(bool active);
bool calibration_command_active();
uint8_t occupied_anchor_mask();
std::optional<uint8_t> reserve_measurement_anchor(uint8_t preferred);
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
void configure_pressure_monitor(const Score &reference, float low_velocity_mm_s, float high_velocity_mm_s, bool flexible = false);
/// Select which print-time INDX loadcell observations may become faults.
/// Presence covers forward E motion with no pressure rise; movement covers a
/// pressure collapse or melt-flow breakout after pressure was established.
void set_pressure_monitor_detection(bool presence, bool movement);
void reset_pressure_monitor();
void suspend_pressure_monitor(bool suspend);
/// Nestable exclusion for maintenance extrusion. Resume only this scope's
/// hold, on every exit path, without changing the user's detection settings.
class PressureMonitorSuspension {
public:
    PressureMonitorSuspension() { suspend_pressure_monitor(true); }
    ~PressureMonitorSuspension() { suspend_pressure_monitor(false); }
    PressureMonitorSuspension(const PressureMonitorSuspension &) = delete;
    PressureMonitorSuspension &operator=(const PressureMonitorSuspension &) = delete;
};
ExtrusionFault consume_extrusion_fault();
void acknowledge_extrusion_fault();

} // namespace buddy::extrusion_calibration

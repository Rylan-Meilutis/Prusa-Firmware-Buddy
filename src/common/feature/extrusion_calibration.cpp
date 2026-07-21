#include "extrusion_calibration.hpp"

#include <algorithm>
#include <cmath>

namespace buddy::extrusion_calibration {

namespace {
Capture instance;
std::array<Result, max_logical_filaments> results {};
std::atomic<uint8_t> calibration_command_depth { 0 };
std::atomic_uint8_t anchors { 0 };
float profile_pressure_advance = NAN;
float calibrated_pressure_advance = NAN;
std::atomic_bool monitor_enabled { false };
std::atomic_uint8_t monitor_suspend_count { 0 };
std::atomic<ExtrusionFault> monitor_fault { ExtrusionFault::none };
std::atomic_bool monitor_fault_suspended { false };
float monitor_sign = 1;
float monitor_low_pressure = 0;
float monitor_pressure_per_velocity = 0;
float monitor_noise = 1;
float monitor_low_velocity = 0.8f;
uint32_t monitor_last_time = 0;
float monitor_last_e = 0;
float monitor_filtered_load = 0;
float monitor_idle_baseline = 0;
float monitor_forward_time = 0;
float monitor_forward_e = 0;
float monitor_peak_pressure = 0;
float monitor_bad_time = 0;
float monitor_breakout_time = 0;
bool monitor_breakout_seen = false;
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
    Score result;
    result.sample_count = static_cast<uint16_t>(std::min(n, size_t(std::numeric_limits<uint16_t>::max())));
    result.capture_overflow = size() >= samples_.size();
    if (n < 32) {
        return result;
    }

    // Firmware-side form of PrusaPATuner's bd_pressure step decomposition.
    // Executed E-step positions locate the real slow/fast transitions. For
    // each transition compare the first 150 ms against settled plateaus and
    // penalize normalized error area, over/undershoot and settling time.
    float cost = 0, cost_squared = 0, load_sum = 0, noise_sum = 0, low_sum = 0, high_sum = 0;
    size_t noise_observations = 0;
    size_t used = 0;
    size_t last_transition = 0;
    for (size_t i = 3; i + 40 < n; ++i) {
        const float dt0 = static_cast<float>(samples_[i].time_us - samples_[i - 1].time_us) * 1e-6f;
        const float dt1 = static_cast<float>(samples_[i - 1].time_us - samples_[i - 2].time_us) * 1e-6f;
        if (dt0 <= 0 || dt1 <= 0) {
            ++result.rejected_timing;
            continue;
        }
        const float v0 = (samples_[i].e_position_mm - samples_[i - 1].e_position_mm) / dt0;
        const float v1 = (samples_[i - 1].e_position_mm - samples_[i - 2].e_position_mm) / dt1;
        if (std::abs(v0 - v1) < 1.0f || i - last_transition < 12) {
            noise_sum += std::abs(samples_[i].load_g - samples_[i - 1].load_g);
            ++noise_observations;
            continue;
        }
        ++result.transitions_detected;
        last_transition = i;
        constexpr size_t plateau_n = 10;
        float before = 0, after = 0, local_noise = 0;
        for (size_t j = i - 13; j < i - 3; ++j) {
            before += samples_[j].load_g;
            if (j > i - 13) local_noise += std::abs(samples_[j].load_g - samples_[j - 1].load_g);
        }
        for (size_t j = i + 25; j < i + 35; ++j) after += samples_[j].load_g;
        before /= plateau_n;
        after /= plateau_n;
        const float amplitude = std::abs(after - before);
        const float noise = local_noise / (plateau_n - 1);
        result.strongest_transition = std::max(result.strongest_transition, amplitude);
        result.highest_transition_noise = std::max(result.highest_transition_noise, noise);
        if (amplitude < std::max(1.0f, noise * 4.0f)) {
            ++result.rejected_low_signal;
            continue;
        }

        const float direction = after >= before ? 1.0f : -1.0f;
        float area = 0, excursion = 0;
        size_t settled_at = 27;
        for (size_t j = 0; j < 27 && i + j < n; ++j) {
            const float normalized_error = (samples_[i + j].load_g - after) / amplitude;
            area += std::abs(normalized_error);
            excursion = std::max(excursion, direction * (samples_[i + j].load_g - after) / amplitude);
            if (settled_at == 27 && std::abs(normalized_error) < 0.1f) settled_at = j;
        }
        const float transition_cost = area / 27.0f + 2.0f * std::max(0.0f, excursion) + float(settled_at) / 27.0f;
        cost += transition_cost;
        cost_squared += transition_cost * transition_cost;
        load_sum += amplitude;
        noise_sum += noise;
        ++noise_observations;
        if (v0 > v1) {
            high_sum += after;
            low_sum += before;
        } else {
            low_sum += after;
            high_sum += before;
        }
        ++used;
    }

    result.transitions_used = static_cast<uint16_t>(std::min(used, size_t(std::numeric_limits<uint16_t>::max())));
    result.valid = used >= 4 && size() < samples_.size();
    if (used > 0) {
        result.transient = cost / used;
        result.transient_stddev = std::sqrt(std::max(0.0f, cost_squared / used - result.transient * result.transient));
        result.mean_load = load_sum / used;
        // Background deltas and transition-local noise are individual noise
        // observations. Dividing their sum only by the transition count made
        // longer captures look artificially noisy and forced false retries.
        result.noise = noise_observations ? noise_sum / noise_observations : std::numeric_limits<float>::infinity();
        result.low_load = low_sum / used;
        result.high_load = high_sum / used;
    }
    return result;
}

float Capture::noise_floor() const {
    const size_t n = std::min(size(), samples_.size());
    if (n < 16) return std::numeric_limits<float>::infinity();
    float mean = 0;
    for (size_t i = 0; i < n; ++i) mean += samples_[i].load_g;
    mean /= n;
    float variance = 0;
    for (size_t i = 0; i < n; ++i) {
        const float error = samples_[i].load_g - mean;
        variance += error * error;
    }
    return std::sqrt(variance / (n - 1));
}

Capture &capture() { return instance; }

void record_loadcell_sample(const uint32_t time_us, const float load_g, const float e_position_mm) {
    instance.record(time_us, load_g, e_position_mm);

    if (!monitor_enabled.load(std::memory_order_acquire) || monitor_suspend_count.load(std::memory_order_relaxed) != 0) return;
    if (!monitor_last_time) {
        monitor_last_time = time_us;
        monitor_last_e = e_position_mm;
        monitor_filtered_load = monitor_idle_baseline = load_g;
        return;
    }
    const float dt = float(time_us - monitor_last_time) * 1e-6f;
    const float de = e_position_mm - monitor_last_e;
    monitor_last_time = time_us;
    monitor_last_e = e_position_mm;
    if (!(dt > 0 && dt < 0.1f)) return;

    monitor_filtered_load += 0.12f * (load_g - monitor_filtered_load);
    const float velocity = de / dt;
    if (velocity <= 0.05f) {
        monitor_idle_baseline += 0.004f * (monitor_filtered_load - monitor_idle_baseline);
        monitor_forward_time = monitor_forward_e = monitor_bad_time = monitor_breakout_time = 0;
        monitor_breakout_seen = false;
        monitor_peak_pressure = 0;
        return;
    }

    monitor_forward_time += dt;
    monitor_forward_e += std::max(0.0f, de);
    const float pressure = monitor_sign * (monitor_filtered_load - monitor_idle_baseline);
    const float expected = std::max(monitor_noise * 5.0f,
        monitor_low_pressure + monitor_pressure_per_velocity * std::max(0.0f, velocity - monitor_low_velocity));
    monitor_peak_pressure = std::max(monitor_peak_pressure * 0.995f, pressure);

    // Ignore priming, seam starts, tiny infill segments and the initial
    // pressure transient. A real loss of filament motion is sustained.
    const bool qualified = monitor_forward_time > 2.0f && monitor_forward_e > 2.0f;
    const bool pressure_missing = pressure < std::max(monitor_noise * 5.0f, expected * 0.25f);
    const bool pressure_collapsed = monitor_peak_pressure > expected * 0.65f && pressure < monitor_peak_pressure * 0.30f;
    monitor_bad_time = (qualified && (pressure_missing || pressure_collapsed)) ? monitor_bad_time + dt : 0;

    // PATuner max-flow markers: sustained force departure above the healthy
    // pressure curve is the soft melt-limit signal; a later collapse is skip.
    const bool breakout = qualified && velocity > monitor_low_velocity * 2.0f
        && pressure > expected + std::max(monitor_noise * 8.0f, monitor_low_pressure * 1.5f);
    monitor_breakout_time = breakout ? monitor_breakout_time + dt : 0;
    monitor_breakout_seen = monitor_breakout_seen || monitor_breakout_time > 1.0f;

    ExtrusionFault wanted = ExtrusionFault::none;
    // High pressure by itself is not proof of a stuck filament: a deliberately
    // thick, slow purge line can sustain it while extruding normally. Treat it
    // as a soft melt-limit marker, and pause only if pressure subsequently
    // collapses or fails to rise. This preserves real max-flow protection
    // without turning a healthy purge into an immediate M1601 fault.
    // Do not react to a single transition or one slow/thick extrusion move.
    // Require a full three seconds of continuously bad pressure evidence after
    // the initial motion qualification; any healthy sample resets bad_time.
    if (monitor_bad_time > 3.0f) {
        wanted = pressure_collapsed && monitor_breakout_seen
            ? ExtrusionFault::flow_breakout
            : pressure_collapsed ? ExtrusionFault::pressure_collapse : ExtrusionFault::no_pressure_rise;
    }
    if (wanted != ExtrusionFault::none) {
        ExtrusionFault expected_none = ExtrusionFault::none;
        monitor_fault.compare_exchange_strong(expected_none, wanted, std::memory_order_acq_rel);
    }
}

void reset_job_results() {
    results = {};
    profile_pressure_advance = NAN;
    calibrated_pressure_advance = NAN;
    reset_pressure_monitor();
    // Anchor occupancy intentionally survives job boundaries. Physical debris
    // does not disappear when a print ends.
}

void note_profile_pressure_advance(const float value) { profile_pressure_advance = value; }

float profile_pressure_advance_or(const float fallback) {
    return std::isfinite(profile_pressure_advance) ? profile_pressure_advance : fallback;
}

void configure_pressure_monitor(const Score &reference, const float low_velocity_mm_s, const float high_velocity_mm_s) {
    monitor_enabled.store(false, std::memory_order_release);
    const float delta = reference.high_load - reference.low_load;
    monitor_sign = delta >= 0 ? 1.0f : -1.0f;
    // Runtime pressure is measured relative to a freshly learned idle
    // baseline. Do not compare that delta with the calibration's absolute
    // (tared) low-speed load; doing so classified every extrusion as missing
    // pressure. Derive the expected response from the measured step instead.
    monitor_low_pressure = std::max(reference.noise * 5.0f, std::abs(delta) * 0.15f);
    monitor_pressure_per_velocity = std::max(0.0f, (std::abs(delta)) / std::max(0.1f, high_velocity_mm_s - low_velocity_mm_s));
    monitor_noise = std::max(0.25f, reference.noise);
    monitor_low_velocity = low_velocity_mm_s;
    monitor_last_time = 0;
    monitor_forward_time = 0;
    monitor_forward_e = 0;
    monitor_bad_time = 0;
    monitor_breakout_time = 0;
    monitor_breakout_seen = false;
    monitor_peak_pressure = 0;
    monitor_fault.store(ExtrusionFault::none, std::memory_order_release);
    monitor_enabled.store(reference.valid, std::memory_order_release);
}

void reset_pressure_monitor() {
    monitor_enabled.store(false, std::memory_order_release);
    monitor_suspend_count.store(0, std::memory_order_relaxed);
    monitor_fault.store(ExtrusionFault::none, std::memory_order_release);
    monitor_fault_suspended.store(false, std::memory_order_release);
    monitor_last_time = 0;
}

void suspend_pressure_monitor(const bool suspend) {
    if (suspend) {
        monitor_suspend_count.fetch_add(1, std::memory_order_acq_rel);
        return;
    }
    uint8_t count = monitor_suspend_count.load(std::memory_order_acquire);
    while (count) {
        if (!monitor_suspend_count.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel)) continue;
        if (count == 1) {
            // Loading/unloading can move E a long distance without nozzle
            // pressure. Re-arm from the next real sample and grant the full
            // continuous-extrusion qualification window after the operation.
            monitor_last_time = 0;
            monitor_forward_time = monitor_forward_e = monitor_bad_time = monitor_breakout_time = 0;
            monitor_breakout_seen = false;
            monitor_peak_pressure = 0;
        }
        break;
    }
}

ExtrusionFault consume_extrusion_fault() {
    const auto fault = monitor_fault.exchange(ExtrusionFault::none, std::memory_order_acq_rel);
    if (fault != ExtrusionFault::none && !monitor_fault_suspended.exchange(true, std::memory_order_acq_rel))
        monitor_suspend_count.fetch_add(1, std::memory_order_acq_rel);
    return fault;
}

void acknowledge_extrusion_fault() {
    if (monitor_fault_suspended.exchange(false, std::memory_order_acq_rel))
        suspend_pressure_monitor(false);
    monitor_fault.store(ExtrusionFault::none, std::memory_order_release);
    monitor_last_time = 0;
    monitor_forward_time = monitor_forward_e = monitor_bad_time = monitor_breakout_time = 0;
    monitor_breakout_seen = false;
    monitor_peak_pressure = 0;
}

const Result *job_result(const size_t logical_filament) {
    return logical_filament < results.size() && results[logical_filament].valid ? &results[logical_filament] : nullptr;
}

void set_job_result(const size_t logical_filament, const Result &result) {
    if (logical_filament < results.size()) {
        results[logical_filament] = result;
        if (result.valid) calibrated_pressure_advance = result.pressure_advance;
    }
}

float calibrated_pressure_advance_or(const float fallback) {
    return std::isfinite(calibrated_pressure_advance) ? calibrated_pressure_advance : fallback;
}

void set_calibration_command_active(const bool active) {
    if (active) {
        calibration_command_depth.fetch_add(1, std::memory_order_acq_rel);
        return;
    }
    uint8_t depth = calibration_command_depth.load(std::memory_order_acquire);
    while (depth && !calibration_command_depth.compare_exchange_weak(depth, depth - 1, std::memory_order_acq_rel)) {}
}

bool calibration_command_active() {
    return calibration_command_depth.load(std::memory_order_acquire) != 0;
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

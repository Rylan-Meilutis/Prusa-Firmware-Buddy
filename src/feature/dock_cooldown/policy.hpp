#pragma once

#include <cstdint>

namespace buddy::dock_cooldown {

// Marlin-task owned. No print/job-state dependency: cooling survives finishing,
// cancellation and idle. This is timed cooling, not a temperature guarantee.
class Policy {
public:
    void configure(bool enabled, uint32_t seconds, uint32_t now) {
        step(now);
        enabled_ = enabled;
        seconds_ = seconds;
    }

    void request_pwm(uint8_t pwm) { manual_pwm_ = pwm; }
    uint8_t requested_pwm() const { return manual_pwm_; }

    void hot_park(uint8_t tool, uint32_t now) {
        if (tool >= 8) {
            return;
        }
        step(now);
        pending_ |= uint8_t(1u << tool);
        elapsed_ms_ = 0;
        timer_active_ = true;
    }

    void picked(uint8_t tool) {
        if (tool < 8) {
            pending_ &= uint8_t(~(1u << tool));
        }
    }

    void step(uint32_t now) {
        if (timer_active_) {
            elapsed_ms_ += uint32_t(now - last_step_ms_);
        }
        last_step_ms_ = now;
        if (timer_active_ && elapsed_ms_ >= uint64_t(seconds_) * 1000) {
            timer_active_ = false;
            pending_ = 0;
        }
    }

    uint8_t pwm() const { return enabled_ ? (outstanding() ? 255 : 0) : manual_pwm_; }
    bool enabled() const { return enabled_; }
    bool outstanding() const { return timer_active_; }
    uint32_t seconds() const { return seconds_; }
    uint8_t pending() const { return pending_; }
    uint32_t remaining_s() const {
        const uint64_t minimum_ms = uint64_t(seconds_) * 1000;
        return timer_active_ && elapsed_ms_ < minimum_ms ? uint32_t((minimum_ms - elapsed_ms_ + 999) / 1000) : 0;
    }

private:
    bool enabled_ = false;
    bool timer_active_ = false;
    uint8_t manual_pwm_ = 0;
    uint8_t pending_ = 0;
    uint32_t seconds_ = 180;
    uint32_t last_step_ms_ = 0;
    uint64_t elapsed_ms_ = 0;
};

} // namespace buddy::dock_cooldown

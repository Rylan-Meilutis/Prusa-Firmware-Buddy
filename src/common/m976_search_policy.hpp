#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace buddy::m976_search {
inline constexpr float verification_step = 0.002f;

// Repeated measurements at the same PA are independent evidence, not another
// chance to cherry-pick a smaller cost. Pool the transition distribution,
// including disagreement between captures; never manufacture repeatability
// by averaging away the observed variance or the worst noise floor.
template <typename Score>
Score combine_repeated_scores(const Score &a, const Score &b) {
    Score result = b;
    const float wa = a.transitions_used, wb = b.transitions_used;
    const float count = wa + wb;
    const auto mean = [&](float x, float y) { return (wa * x + wb * y) / count; };
    const auto sum = [](uint16_t x, uint16_t y) {
        return static_cast<uint16_t>(std::min<uint32_t>(uint32_t(x) + y, std::numeric_limits<uint16_t>::max()));
    };
    if (!a.valid || !b.valid || count == 0) {
        result.valid = false;
        return result;
    }
    result.transient = mean(a.transient, b.transient);
    const float da = a.transient - result.transient, db = b.transient - result.transient;
    result.transient_stddev = std::sqrt(mean(a.transient_stddev * a.transient_stddev + da * da,
        b.transient_stddev * b.transient_stddev + db * db));
    result.mean_load = mean(a.mean_load, b.mean_load);
    result.low_load = mean(a.low_load, b.low_load);
    result.high_load = mean(a.high_load, b.high_load);
    result.noise = std::max(a.noise, b.noise);
    result.strongest_transition = std::max(a.strongest_transition, b.strongest_transition);
    result.highest_transition_noise = std::max(a.highest_transition_noise, b.highest_transition_noise);
    result.transitions_used = sum(a.transitions_used, b.transitions_used);
    result.transitions_detected = sum(a.transitions_detected, b.transitions_detected);
    result.sample_count = sum(a.sample_count, b.sample_count);
    result.rejected_low_signal = sum(a.rejected_low_signal, b.rejected_low_signal);
    result.rejected_timing = sum(a.rejected_timing, b.rejected_timing);
    result.capture_overflow = a.capture_overflow || b.capture_overflow;
    result.valid = !result.capture_overflow;
    return result;
}

// A warm start is only a search hint. Both neighbours and an independent
// repeated centre must support it before the normal acceptance gates apply.
inline bool verified_minimum(float seed, float limit, float center, float lower, float upper, float repeated,
    bool quality_ok) {
    return quality_ok && std::isfinite(seed) && seed >= verification_step && seed <= limit - verification_step
        && std::isfinite(center) && std::isfinite(lower) && std::isfinite(upper) && std::isfinite(repeated)
        && std::max(center, repeated) < std::min(lower, upper)
        && std::abs(center - repeated) <= std::max(0.01f, 0.10f * std::abs(center));
}

struct SignalGuard {
    uint8_t weak = 0;
    const char *failure = nullptr;

    void observe(uint16_t samples, bool overflow, float peak, float noise, float minimum_snr) {
        if (!samples) {
            failure = "no_capture_data";
        } else if (overflow) {
            failure = "capture_overflow";
        } else if (!std::isfinite(peak) || !std::isfinite(noise) || !std::isfinite(minimum_snr)
            || peak < std::max(0.25f, noise) * minimum_snr) {
            if (++weak >= 3) {
                failure = "repeated_low_signal";
            }
        } else {
            weak = 0;
        }
    }
};
} // namespace buddy::m976_search

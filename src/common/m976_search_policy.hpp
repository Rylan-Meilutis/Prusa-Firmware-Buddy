#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace buddy::m976_search {
inline constexpr float verification_step = 0.002f;

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

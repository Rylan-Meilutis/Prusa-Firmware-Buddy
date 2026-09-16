#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <cmath>

namespace buddy::chamber_heating {

constexpr float hysteresis_c = 1.0f;

// Own only an idle chamber request's temporary bed boost. Explicit bed
// commands (including Off and safety shutdown) revoke ownership until the
// next chamber request, even when they happen to repeat our current target.
class IdleBedAssist {
public:
    void request() { inhibited_ = false; }
    void override_bed() {
        inhibited_ = true;
        previous_.reset();
        applied_.reset();
    }
    std::optional<int16_t> update(std::optional<float> current, std::optional<float> target,
        bool printing, int16_t bed, int16_t bed_limit) {
        if (printing) {
            inhibited_ = true;
        }
        if (applied_ && bed != *applied_) {
            override_bed();
        }
        const bool heat = !inhibited_ && current && target && std::isfinite(*current)
            && std::isfinite(*target) && *target > 0
            && *current < *target - (applied_ ? 0.0f : hysteresis_c);
        if (!heat) {
            const auto restore = previous_;
            previous_.reset();
            applied_.reset();
            return restore && *restore != bed ? restore : std::nullopt;
        }
        // Conservative idle boost, additionally bounded by this machine's
        // normal thermal limit. Never lower a pre-existing manual bed target.
        const auto boost = static_cast<int16_t>(std::min(*target + 40.0f, float(std::min<int16_t>(100, bed_limit))));
        if (!previous_) {
            previous_ = bed;
        }
        applied_ = std::max(*previous_, boost);
        return *applied_ != bed ? applied_ : std::nullopt;
    }

private:
    bool inhibited_ = true;
    std::optional<int16_t> previous_;
    std::optional<int16_t> applied_;
};

constexpr bool should_assist(std::optional<float> current, std::optional<float> target, bool printing, bool blocking_heat_wait) {
    return current.has_value() && target.has_value() && *target > 0
        && *current < *target - hysteresis_c
        && (!printing || blocking_heat_wait);
}

template <class T>
constexpr T assisted_output(T previous, T minimum) {
    return std::max(previous, minimum);
}

} // namespace buddy::chamber_heating

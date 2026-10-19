#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace buddy::pa_temperature {
inline constexpr size_t capacity = 8;

struct Point {
    int16_t temperature = 0;
    float pa = 0, max_flow = 0, confidence = 0;
    float low_load = 0, high_load = 0, noise = 0;
    bool reference_valid = false, flexible = false;
};

struct Estimate {
    Point point {};
    float slope = 0, intercept = 0, rmse = 0;
    const char *method = "unavailable";
    bool valid = false;
};

class Model {
public:
    std::array<Point, capacity> points {};
    size_t count = 0;

    // Search hint only, never a reusable calibration or pressure reference.
    // Permit a small temperature extension; a noisy fit uses the nearest
    // measured value instead. The caller must independently verify the hint.
    float search_seed(int16_t temperature) const {
        const auto estimate = evaluate(temperature);
        if (estimate.valid) {
            return estimate.point.pa;
        }
        if (!count || temperature < 170 || temperature > 300) {
            return NAN;
        }
        const auto &nearest = temperature < points[0].temperature ? points[0] : points[count - 1];
        if (std::abs(int(temperature) - nearest.temperature) > 15) {
            return NAN;
        }
        const auto fit = evaluate(nearest.temperature);
        const float delta = count > 1 && fit.rmse <= 0.005f
            ? std::clamp(fit.slope * (temperature - nearest.temperature), -0.02f, 0.02f)
            : 0;
        return std::clamp(nearest.pa + delta, 0.0f, 0.5f);
    }

    bool add(const Point &point) {
        if (point.temperature < 170 || point.temperature > 300
            || !std::isfinite(point.pa) || point.pa < 0 || point.pa > 0.5f
            || !std::isfinite(point.confidence) || point.confidence <= 0 || point.confidence > 1
            || !std::isfinite(point.max_flow) || point.max_flow <= 0
            || !std::isfinite(point.low_load) || !std::isfinite(point.high_load)
            || !std::isfinite(point.noise) || point.noise < 0) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (points[i].temperature == point.temperature) {
                points[i] = point;
                return true;
            }
        }
        if (count == capacity) {
            return false;
        }
        points[count++] = point;
        std::sort(points.begin(), points.begin() + count, [](const auto &a, const auto &b) { return a.temperature < b.temperature; });
        return true;
    }

    Estimate evaluate(int16_t temperature) const {
        Estimate result;
        if (!count || temperature < points[0].temperature || temperature > points[count - 1].temperature) {
            return result; // Never extrapolate, including standby/probing targets.
        }
        // Centering the weighted regression avoids cancellation at ~250 C.
        float weights = 0, mean_t = 0, mean_pa = 0;
        for (size_t i = 0; i < count; ++i) {
            const float weight = points[i].confidence * points[i].confidence;
            weights += weight;
            mean_t += weight * points[i].temperature;
            mean_pa += weight * points[i].pa;
        }
        mean_t /= weights;
        mean_pa /= weights;
        float covariance = 0, variance = 0;
        for (size_t i = 0; i < count; ++i) {
            const float weight = points[i].confidence * points[i].confidence;
            const float dt = points[i].temperature - mean_t;
            covariance += weight * dt * (points[i].pa - mean_pa);
            variance += weight * dt * dt;
        }
        result.slope = variance > 0 ? covariance / variance : 0;
        result.intercept = mean_pa - result.slope * mean_t;
        float error = 0;
        for (size_t i = 0; i < count; ++i) {
            const float delta = points[i].pa - (mean_pa + result.slope * (points[i].temperature - mean_t));
            error += points[i].confidence * points[i].confidence * delta * delta;
        }
        result.rmse = std::sqrt(error / weights);
        for (size_t i = 0; i < count; ++i) {
            if (points[i].temperature == temperature) {
                result.point = points[i];
                result.method = count == 1 ? "single" : "measured";
                result.valid = true;
                return result;
            }
        }
        for (size_t i = 1; i < count; ++i) {
            if (temperature > points[i].temperature) {
                continue;
            }
            const auto &lo = points[i - 1], &hi = points[i];
            const float fraction = float(temperature - lo.temperature) / (hi.temperature - lo.temperature);
            const auto interpolate = [&](float a, float b) { return a + fraction * (b - a); };
            result.point = lo;
            result.point.temperature = temperature;
            // Poor linear agreement falls back to local measured endpoints.
            const bool use_fit = result.rmse <= 0.005f;
            const float pa = use_fit ? mean_pa + result.slope * (temperature - mean_t) : interpolate(lo.pa, hi.pa);
            result.point.pa = std::clamp(pa, std::min(lo.pa, hi.pa), std::max(lo.pa, hi.pa));
            result.point.max_flow = std::min(lo.max_flow, hi.max_flow);
            result.point.confidence = std::min(lo.confidence, hi.confidence);
            result.point.low_load = interpolate(lo.low_load, hi.low_load);
            result.point.high_load = interpolate(lo.high_load, hi.high_load);
            result.point.noise = std::max(lo.noise, hi.noise);
            result.point.reference_valid = lo.reference_valid && hi.reference_valid;
            result.method = use_fit ? "regression" : "piecewise";
            result.valid = lo.flexible == hi.flexible;
            return result;
        }
        return result;
    }
};
} // namespace buddy::pa_temperature

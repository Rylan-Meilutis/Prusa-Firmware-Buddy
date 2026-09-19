#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace buddy::pa_cache {

inline constexpr uint32_t record_version = 2;

// Versioned, fixed-size flash record. No pointers, heap allocations or raw
// FilamentType/Score object representations are persisted.
struct Key {
    std::array<char, 32> profile {};
    std::array<char, 16> material {};
    uint32_t color = 0;
    float nozzle = 0;
    int16_t temperature = 0;
    int16_t profile_temperature = 0;
    uint8_t manufacturer = 0;
    uint8_t physical_tool = 0;
    uint8_t color_valid = 0;
    uint8_t confidence_floor = 0;
    float minimum_snr = 0;
    bool flexible = false;
    bool operator==(const Key &) const = default;
};

struct Record {
    Key key {};
    float pa = 0;
    float max_flow = 0;
    float confidence = 0;
    float low_load = 0;
    float high_load = 0;
    float noise = 0;
    uint32_t version = 0;
    bool operator==(const Record &) const = default;

    bool matches(const Key &requested) const {
        return version == record_version && key == requested
            && std::isfinite(pa) && pa >= 0 && pa <= 0.5f
            && std::isfinite(max_flow) && max_flow > 0
            && std::isfinite(confidence) && confidence >= key.confidence_floor / 100.f && confidence <= 1
            && std::isfinite(low_load) && std::isfinite(high_load)
            && std::isfinite(noise) && noise >= 0;
    }
};
} // namespace buddy::pa_cache

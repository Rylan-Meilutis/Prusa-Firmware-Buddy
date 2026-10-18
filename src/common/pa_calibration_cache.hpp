#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <pa_temperature_model.hpp>

namespace buddy::pa_cache {

// Flexible excitation/scoring changed; old pressure references use different
// velocities and must not be interpreted with the new monitor policy.
inline constexpr uint32_t record_version = 3;

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

    // nullptr means reusable. Keep inspection and actual cache acceptance on
    // exactly the same predicate; reporting must never relax a safety gate.
    const char *rejection_reason(const Key &requested) const {
        if (version != record_version) {
            return "record_version_changed";
        }
        if (key.profile != requested.profile) {
            return "profile_changed";
        }
        if (key.material != requested.material) {
            return "material_changed";
        }
        if (key.color != requested.color || key.color_valid != requested.color_valid) {
            return "color_changed";
        }
        if (key.manufacturer != requested.manufacturer) {
            return "manufacturer_changed";
        }
        if (key.physical_tool != requested.physical_tool) {
            return "physical_tool_changed";
        }
        if (key.nozzle != requested.nozzle) {
            return "nozzle_changed";
        }
        if (key.temperature != requested.temperature) {
            return "calibration_temperature_changed";
        }
        if (key.profile_temperature != requested.profile_temperature) {
            return "profile_temperature_changed";
        }
        if (key.confidence_floor != requested.confidence_floor) {
            return "confidence_floor_changed";
        }
        if (key.minimum_snr != requested.minimum_snr) {
            return "minimum_snr_changed";
        }
        if (key.flexible != requested.flexible) {
            return "flexible_policy_changed";
        }
        if (!std::isfinite(pa) || pa < 0 || pa > 0.5f) {
            return "invalid_pa";
        }
        if (!std::isfinite(max_flow) || max_flow <= 0) {
            return "invalid_max_flow";
        }
        if (!std::isfinite(confidence) || confidence < key.confidence_floor / 100.f || confidence > 1) {
            return "invalid_confidence";
        }
        if (!std::isfinite(low_load) || !std::isfinite(high_load) || !std::isfinite(noise) || noise < 0) {
            return "invalid_pressure_reference";
        }
        return nullptr;
    }

    bool matches(const Key &requested) const {
        return rejection_reason(requested) == nullptr;
    }
};

struct Bank {
    std::array<Record, pa_temperature::capacity> records {};
    uint32_t version = 1;
    uint8_t count = 0;
    bool operator==(const Bank &) const = default;

    const Record *find(const Key &key, const char *&reason) const {
        reason = "missing_record";
        const Record *nearest = nullptr;
        int distance = 1000;
        for (size_t i = 0; i < count; ++i) {
            if (records[i].matches(key)) {
                reason = nullptr;
                return &records[i];
            }
            const int delta = std::abs(int(records[i].key.temperature) - key.temperature);
            if (delta < distance) {
                distance = delta;
                nearest = &records[i];
            }
        }
        if (nearest) {
            reason = nearest->rejection_reason(key);
        }
        return nullptr;
    }

    bool insert(const Record &record) {
        if (!record.matches(record.key)) {
            return false;
        }
        if (count) {
            auto same_identity = record.key;
            same_identity.temperature = records[0].key.temperature;
            if (!records[0].matches(same_identity)) {
                *this = {}; // New spool/nozzle/quality policy: never mix fits.
            }
        }
        for (size_t i = 0; i < count; ++i) {
            if (records[i].key.temperature == record.key.temperature) {
                records[i] = record;
                return true;
            }
        }
        if (count == records.size()) {
            // Bounded FIFO replacement: preserve the other temperatures,
            // rather than making every future new temperature a cache miss.
            std::move(records.begin() + 1, records.end(), records.begin());
            --count;
        }
        records[count++] = record;
        return true;
    }
};
} // namespace buddy::pa_cache

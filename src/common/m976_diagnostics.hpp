#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace buddy::m976_diagnostics {

// Deliberately separate from job results: cancellation and a new job must not
// erase the explanation for the last attempt. RAM only; reset on reboot.
struct Record {
    uint8_t tool = 255; // No physical tool associated with an unattempted slot.
    int16_t temperature = 0;
    const char *outcome = "never_run";
    const char *reason = "no_attempt_this_boot";
    const char *cache = "not_checked";
    float pa = std::numeric_limits<float>::quiet_NaN();
    float candidate_pa = std::numeric_limits<float>::quiet_NaN();
    float confidence = std::numeric_limits<float>::quiet_NaN();
    float minimum_confidence = std::numeric_limits<float>::quiet_NaN();
    float snr = std::numeric_limits<float>::quiet_NaN();
    float minimum_snr = std::numeric_limits<float>::quiet_NaN();
    float noise = std::numeric_limits<float>::quiet_NaN();
    float separation = std::numeric_limits<float>::quiet_NaN();
    uint16_t valid_candidates = 0;
    uint16_t samples = 0;
    uint16_t transitions = 0;
    uint16_t rejected_low_signal = 0;
    uint16_t rejected_timing = 0;
    bool capture_overflow = false;
};

class History {
public:
    std::array<Record, 8> records {};
    const char *request_error = "none";

    Record &begin(uint8_t tool, uint8_t slot, int16_t temperature) {
        auto &record = records[slot];
        record = {};
        record.tool = tool;
        record.temperature = temperature;
        record.outcome = "running";
        record.reason = "preparing";
        return record;
    }
};

class Attempt {
    Record &record;

public:
    explicit Attempt(Record &record)
        : record(record) {}
    ~Attempt() {
        if (record.outcome == std::string_view("running")) {
            record.outcome = "failed";
        }
    }
};

} // namespace buddy::m976_diagnostics

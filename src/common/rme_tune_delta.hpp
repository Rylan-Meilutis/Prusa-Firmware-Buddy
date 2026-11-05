#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rme_tune {
// Cache only the values actually serialized. Stable names keep optional
// platform sensors from shifting cache slots when a tool appears/disappears.
class Delta {
public:
    void reset() {
        initialized = false;
        valid.reset();
    }
    bool begin(std::optional<uint32_t> since) {
        partial = initialized && since && *since == sequence_;
        initialized = true;
        ++sequence_;
        return partial;
    }
    uint32_t sequence() const { return sequence_; }
    bool changed(std::string_view key, int64_t value) {
        for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] != key) {
                continue;
            }
            const bool send = !partial || !valid[i] || values[i] != value;
            valid[i] = true;
            values[i] = value;
            return send;
        }
        return true; // A future uncached field is safe, never silently omitted.
    }

private:
    static constexpr std::array keys {
        "speed", "fan", "dock_fan", "dock_rpm", "heatbreak_pwm", "heatbreak_rpm", "part_fan_rpm",
        "controller_fan_pwm", "controller_fan_rpm", "heatbreak_temp", "buddy_mcu", "buddy_board",
        "head_mcu", "head_board", "head_ambient", "chamber_temp", "progress_percent", "progress_remaining",
        "host_percent", "stealth", "auto_pa", "printing", "light", "lcd", "screen_print", "chamber_print",
        "status_print", "material_cooling", "print_filter", "post_filter", "F0", "F1", "F2", "F3", "F4",
        "F5", "F6", "F7", "toolmap"
    };
    std::array<int64_t, keys.size()> values {};
    std::bitset<keys.size()> valid;
    uint32_t sequence_ = 0;
    bool initialized = false, partial = false;
};
} // namespace rme_tune

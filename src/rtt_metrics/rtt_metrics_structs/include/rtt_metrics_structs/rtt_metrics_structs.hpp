///@file
#pragma once

#include <cstdint>

namespace rtt_metrics {
enum class MetricType : uint8_t {
    raw_acceleration,
    loadcell_tared_z,
};

template <typename DataStruct>
struct MetricWrapper {
    MetricType type;
    uint32_t timestamp;
    DataStruct data;
};

/// Payload for MetricType::loadcell_tared_z: the tared Z load
/// (Loadcell::get_tared_z_load) in grams. Already scaled and calibrated on the
/// firmware; tare-relative, so a load change rather than an absolute weight.
struct LoadcellTaredZ {
    float z_load;
};

} // namespace rtt_metrics

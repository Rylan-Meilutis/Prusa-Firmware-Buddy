///@file
#pragma once

#include <cstdint>

namespace rtt_metrics {
enum class MetricType : uint8_t {
    RawAcceleration
};

template <typename DataStruct>
struct MetricWrapper {
    MetricType type;
    uint32_t timestamp;
    DataStruct data;
};

} // namespace rtt_metrics

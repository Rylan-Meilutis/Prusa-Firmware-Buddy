///@file
#include "include/rtt_metrics_segger/peripheries_metrics.hpp"
#include "include/rtt_metrics_segger/rtt_metrics_segger.hpp"

#include <cstring>
#include <span>

#include <rtt_metrics_serialization/peripheries_serialization.hpp>
#include <rtt_metrics_structs/rtt_metrics_structs.hpp>
#include <timing.h>

using namespace rtt_metrics;

namespace {
template <typename DataStruct>
void log_metric_with_data(MetricType type, uint32_t timestamp, const DataStruct &data) {
    const auto serialized = serialize_metric<DataStruct>({
        .type = type,
        .timestamp = timestamp,
        .data = data,
    });
    log_metric(std::as_bytes(std::span { serialized.buffer.data(), serialized.written_size }));
}
} // anonymous namespace

void rtt_metrics::log_accelerometer(const accelerometer::RawAcceleration &raw_acceleration) {
    log_metric_with_data(MetricType::RawAcceleration, ticks_us(),
        raw_acceleration);
}

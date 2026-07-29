///@file
#pragma once

#include <accelerometer/common_structs.hpp>
#include <rtt_metrics_structs/rtt_metrics_structs.hpp>
#include <array>

#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/array.h>
#include <bitsery/brief_syntax/array.h>

namespace rtt_metrics {

template <size_t DataSize>
struct SerializedBuffer {
    std::array<uint8_t, DataSize> buffer;
    size_t written_size;
};

template <typename DataStruct>
auto serialize_metric(const MetricWrapper<DataStruct> &wrapper) {
    SerializedBuffer<sizeof(wrapper)> out;
    using OutputAdapter = bitsery::OutputBufferAdapter<decltype(out.buffer)>;
    out.written_size = bitsery::quickSerialization<OutputAdapter>(OutputAdapter { out.buffer }, wrapper);
    return out;
}

template <typename S, typename DataStruct>
void serialize(S &s, MetricWrapper<DataStruct> &w) {
    s.value1b(w.type);
    s.value4b(w.timestamp);
    s.object(w.data);
}

template <typename S>
void serialize(S &s, LoadcellTaredZ &data) {
    s.value4b(data.z_load);
}

template <typename S>
void serialize(S &s, StepperPositions &data) {
    for (auto &step : data.steps) {
        s.value4b(step);
    }
}

} // namespace rtt_metrics

namespace accelerometer {
template <typename S>
void serialize(S &s, RawAcceleration &data) {
    s.value2b(data.val[0]);
    s.value2b(data.val[1]);
    s.value2b(data.val[2]);
}
} // namespace accelerometer

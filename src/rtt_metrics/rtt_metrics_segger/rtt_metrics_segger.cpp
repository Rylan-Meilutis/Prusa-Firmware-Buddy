///@file
#include "include/rtt_metrics_segger/rtt_metrics_segger.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cobs/cobs.hpp>
#include <utils/byte_utils.hpp>

#include <SEGGER_RTT.h>

using namespace rtt_metrics;

/// Check whether an RTT consumer (debugger) is actively draining the buffer.
///
/// Compares the current RdOff with a snapshot taken after the previous write.
/// - If RdOff advanced → consumer is alive, safe to do a blocking write.
/// - If RdOff is stagnant AND the buffer is full → no consumer, skip the write.
/// - If RdOff is stagnant but buffer has space → proceed (consumer may be slow).
///
/// Interacting with segger internals this way is the only solution I was able to think of.
/// This might cause some data races though since RdOff is volatile.
static bool has_active_consumer(unsigned buffer_index) {
    static unsigned prev_rd_off = 0;
    static bool consumer_present = true;

    const unsigned rd_off = _SEGGER_RTT.aUp[buffer_index].RdOff;

    if (rd_off != prev_rd_off) {
        prev_rd_off = rd_off;
        consumer_present = true;
        return true;
    }

    if (SEGGER_RTT_GetAvailWriteSpace(buffer_index) == 0) {
        consumer_present = false;
        return false;
    }

    return consumer_present;
}

void rtt_metrics::log_metric(Bytes buffer) {
    static uint8_t rtt_buffer_index;
    static char rtt_buffer_data[256];

    if (!rtt_buffer_index) {
        rtt_buffer_index = 2;
        SEGGER_RTT_Init();
        SEGGER_RTT_ConfigUpBuffer(rtt_buffer_index, "metrics", &rtt_buffer_data[0], 256, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    }

    const size_t encoded_buffer_size = cobs::max_encoded_frame_size(buffer.size());
    std::byte encoded[encoded_buffer_size];
    WritableBytes encoded_buffer_span(encoded, encoded_buffer_size);

    Bytes input_buffer(buffer.data(), buffer.size());
    auto encoded_ret = cobs::encode(input_buffer, encoded_buffer_span);
    if (has_active_consumer(rtt_buffer_index)) {
        debug_assert(encoded_ret.has_value());
        SEGGER_RTT_Write(rtt_buffer_index, encoded, encoded_ret.value());
    }
}

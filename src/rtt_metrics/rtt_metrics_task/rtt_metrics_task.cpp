///@file
#include <rtt_metrics_task/rtt_metrics_task.hpp>

#include <freertos/timing.hpp>
#include <rtt_metrics_segger/peripheries_metrics.hpp>
#include <rtt_metrics_segger/rtt_metrics_segger.hpp>

void start_rtt_metrics_task() {
    // Poll cadence for draining the metric queues. Kept short so the queues
    // (see peripheries_metrics.cpp) stay well within their depth at the
    // producers' sampling rates.
    constexpr size_t poll_interval_ms = 1;

    rtt_metrics::init_rtt_metrics();
    for (;;) {
        rtt_metrics::process_rtt_metrics_queue();
        freertos::delay(poll_interval_ms);
    }
}

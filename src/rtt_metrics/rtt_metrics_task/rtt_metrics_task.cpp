///@file
#include <rtt_metrics_task/rtt_metrics_task.hpp>

#include <freertos/timing.hpp>

void start_rtt_metrics_task() {
    for (;;) {
        freertos::delay(50);
    }
}

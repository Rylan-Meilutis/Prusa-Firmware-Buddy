#include "pid_autotune_status.hpp"

#include <algorithm>

namespace {

pid_autotune_status::Snapshot status {
    .heater = pid_autotune_status::Heater::none,
    .active = false,
    .finished = false,
    .failed = false,
    .heating = true,
    .progress = 0,
    .cycle = 0,
    .total_cycles = 0,
    .target = 0,
    .current = 0,
    .p = 0,
    .i = 0,
    .d = 0,
};

uint8_t progress_for(float current, float target, int8_t cycle, int8_t total_cycles) {
    if (total_cycles <= 0) {
        return 0;
    }

    const uint8_t cycle_progress = static_cast<uint8_t>(std::clamp<int>(cycle, 0, total_cycles) * 80 / total_cycles);
    if (cycle > 0) {
        return std::min<uint8_t>(99, 20 + cycle_progress);
    }

    if (target <= 0) {
        return 0;
    }

    return static_cast<uint8_t>(std::clamp<float>((current * 20.0f) / target, 0.0f, 20.0f));
}

} // namespace

namespace pid_autotune_status {

void clear() {
    status = {};
}

void start(Heater heater, float target, int8_t total_cycles) {
    status = {
        .heater = heater,
        .active = true,
        .finished = false,
        .failed = false,
        .heating = true,
        .progress = 0,
        .cycle = 0,
        .total_cycles = total_cycles,
        .target = target,
        .current = 0,
        .p = 0,
        .i = 0,
        .d = 0,
    };
}

void update(float current, int8_t cycle, bool heating) {
    status.current = current;
    status.cycle = cycle;
    status.heating = heating;
    status.progress = progress_for(current, status.target, cycle, status.total_cycles);
}

void finish(bool success, float current, float p, float i, float d) {
    status.current = current;
    status.active = false;
    status.finished = true;
    status.failed = !success;
    status.progress = success ? 100 : status.progress;
    status.p = success ? p : 0;
    status.i = success ? i : 0;
    status.d = success ? d : 0;
}

Snapshot snapshot() {
    return status;
}

} // namespace pid_autotune_status

#pragma once

#include <cstdint>

namespace pid_autotune_status {

enum class Heater : uint8_t {
    none,
    hotend,
    bed,
};

struct Snapshot {
    Heater heater;
    bool active;
    bool finished;
    bool failed;
    bool heating;
    uint8_t progress;
    int8_t cycle;
    int8_t total_cycles;
    float target;
    float current;
    float p;
    float i;
    float d;
};

void clear();
void start(Heater heater, float target, int8_t total_cycles);
void update(float current, int8_t cycle, bool heating);
void finish(bool success, float current, float p, float i, float d);
Snapshot snapshot();

} // namespace pid_autotune_status

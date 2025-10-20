#pragma once

#include <utils/atomic_circular_queue.hpp>

#include <variant>
#include <cstdint>

namespace phase_stepping {
struct Stalled {
    uint32_t timestamp;
    char axis;
    uint32_t number_of_iteration;
};
struct SuddenSpeedChange {
    uint32_t timestamp;
    char axis;
    float original_speed;
    float new_speed;
};
using ProblemEvents = std::variant<Stalled, SuddenSpeedChange>;

/// @brief Queue for debug events
extern AtomicCircularQueue<ProblemEvents, uint16_t, 32> debug_events_queue;
} // namespace phase_stepping

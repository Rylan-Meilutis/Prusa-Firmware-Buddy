/// @file

#include "mmu2_reporting.hpp"

#include <atomic>

namespace MMU2 {

namespace {
std::atomic<uint8_t> pending_serial_host_mmu_events = 0;
} // namespace

void defer_serial_host_mmu_paused() {
    pending_serial_host_mmu_events.fetch_or(static_cast<uint8_t>(SerialHostMmuEvent::paused), std::memory_order_relaxed);
}

void defer_serial_host_mmu_resume() {
    pending_serial_host_mmu_events.fetch_or(static_cast<uint8_t>(SerialHostMmuEvent::resume), std::memory_order_relaxed);
}

SerialHostMmuEvent consume_serial_host_mmu_events() {
    return static_cast<SerialHostMmuEvent>(pending_serial_host_mmu_events.exchange(0, std::memory_order_acq_rel));
}

} // namespace MMU2

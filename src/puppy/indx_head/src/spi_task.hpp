/// @file
#pragma once

#define DO_NOT_CHECK_ATOMIC_LOCK_FREE
#include <utils/atomic_circular_queue.hpp>

#include <fifo_coder/fifo_coder.hpp>

namespace spi_task {
void run();
/// Notify spi task that new accelerometer data is ready
/// @warning Callable only form ISR
void notify_accel_data_ready();
/// Notify spi task that new loadcell data is ready
/// @warning Callable only form ISR
void notify_loadcell_data_ready();

float measured_sampling_rate();

extern AtomicCircularQueue<uint32_t, uint8_t, 128> accel_samples;
extern AtomicCircularQueue<fifo_coder::LoadcellRecord, uint32_t, 32> loadcell_samples;
} // namespace spi_task

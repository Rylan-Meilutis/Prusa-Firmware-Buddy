#pragma once

#include <cstddef>
#include <cstdint>

namespace buddy::task_stack_requirements {

// RME exercises the Marlin serial parser, storage service, and response path
// in one task. A field crash captured the stack pointer 84 bytes below the
// former 1360-word allocation. Keep a substantial, testable guard instead of
// sizing this allocation to the single observed overrun.
inline constexpr size_t marlin_previous_words = 1360;
inline constexpr size_t marlin_words = 1664;
inline constexpr size_t observed_rme_overrun_bytes = 84;
inline constexpr size_t required_guard_bytes = 1024;
inline constexpr size_t marlin_guard_bytes = (marlin_words - marlin_previous_words) * sizeof(uint32_t);

static_assert(marlin_guard_bytes >= required_guard_bytes);
static_assert(marlin_guard_bytes >= observed_rme_overrun_bytes * 4);

// An older INDX build overflowed the unchanged 1664-word allocation while
// restoring Auto PA through littlefs. Keep that historical finding distinct
// from any current boot failure. INDX has spare SRAM, but not enough CCM for
// this guard; its Marlin stack is therefore allocated in ordinary SRAM.
inline constexpr size_t marlin_indx_words = 3072;
inline constexpr size_t observed_indx_overrun_bytes = 176;
inline constexpr size_t marlin_indx_guard_bytes = (marlin_indx_words - marlin_words) * sizeof(uint32_t);
static_assert(marlin_indx_guard_bytes >= 4096);
static_assert(marlin_indx_guard_bytes >= observed_indx_overrun_bytes * 4);

} // namespace buddy::task_stack_requirements

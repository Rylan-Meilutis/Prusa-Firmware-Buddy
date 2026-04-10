#pragma once

#include <cstdint>
#include <cstdlib>

/**
 * @brief Shared between xBuddy and INDX_HEAD
 */
namespace indx_head::errors {
enum class FaultStatusMask : uint16_t {
    // Priority is set in ascending order (higher number -> higher priority)
    no_fault = 0,
    undefined_error = 1,
    nozzle_min_temp = (1 << 1),
    tpis_invalid_timeout = (1 << 2),
    nozzle_max_temp = (1 << 3),
    // Low-level faults (saved to PWR backup register before hal::panic)
    hard_fault = (1 << 4),
    stack_overflow = (1 << 5),
    assert_failed = (1 << 6),
    watchdog_reset = (1 << 8),
};

static constexpr const char *to_string(FaultStatusMask mask) {
    switch (mask) {
    case FaultStatusMask::no_fault:
        return "No fault";
    case FaultStatusMask::nozzle_max_temp:
        return "Nozzle max temperature fault";
    case FaultStatusMask::tpis_invalid_timeout:
        return "TPIS invalid timeout fault";
    default:
        std::abort();
    }
}

} // namespace indx_head::errors

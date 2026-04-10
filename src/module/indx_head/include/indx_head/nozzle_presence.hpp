#pragma once

#include <cstdint>

/**
 * @brief Shared between xBuddy and INDX_HEAD
 */
namespace indx_head {

/// Tristate nozzle presence reported by ping analysis over modbus.
enum class NozzlePresence : uint16_t {
    unknown = 0, ///< Ping analysis has not yet produced a valid result
    present = 1, ///< Ping analysis ran, nozzle detected
    absent = 2, ///< Ping analysis ran, no nozzle detected
};

} // namespace indx_head

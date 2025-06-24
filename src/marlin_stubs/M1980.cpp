#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <feature/door_sensor_calibration/door_sensor_calibration.hpp>

namespace PrusaGcodeSuite {

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1980: Door Sensor Calibration
 *
 * Internal GCode
 *
 *#### Usage
 *
 *    M1980
 *
 */
void M1980() {
    door_sensor_calibration::run();
}

/** @}*/
} // namespace PrusaGcodeSuite

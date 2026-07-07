/// @file
#include "PrusaGcodeSuite.hpp"
#include <optional>
#include <tuple>
#include <feature/prusa/e-stall_detector.h>
#include <Marlin/src/module/planner.h>
#include <option/has_loadcell.h>
#include <config_store/store_instance.hpp>
#include <logging/log.hpp>

LOG_COMPONENT_REF(MarlinServer);

namespace {
enum class IsPermanent { no,
    yes };
enum class Restore { no,
    yes };

#if HAS_LOADCELL()
bool e_stall_detection_enabled() {
    return config_store().stuck_filament_detection.get() || config_store().filament_movement_detection.get();
}

void apply_e_stall_detection() {
    EMotorStallDetector::Instance().SetEnabled(e_stall_detection_enabled());
}

void m591_no_parser(std::optional<bool> opt_enable_stuck, std::optional<bool> opt_enable_movement, IsPermanent is_permanent, Restore restore, std::optional<std::tuple<uint32_t, uint32_t>> ignore) {
    // wait for all previously queued moves to finish to only apply the new settings for moves that follow
    planner.synchronize();

    if (restore == Restore::yes) {
        apply_e_stall_detection();
        log_info(MarlinServer, "E-stall detection %s (restore)", EMotorStallDetector::Instance().Enabled() ? "on" : "off");
        return; // ignore remaining parameters
    }

    if (opt_enable_stuck) {
        if (is_permanent == IsPermanent::yes) {
            config_store().stuck_filament_detection.set(*opt_enable_stuck);
        }
        log_info(MarlinServer, "Filament stuck detection %s%s", *opt_enable_stuck ? "on" : "off", is_permanent == IsPermanent::yes ? " permanent" : nullptr);
    }

    if (opt_enable_movement) {
        if (is_permanent == IsPermanent::yes) {
            config_store().filament_movement_detection.set(*opt_enable_movement);
        }
        log_info(MarlinServer, "Filament movement detection %s%s", *opt_enable_movement ? "on" : "off", is_permanent == IsPermanent::yes ? " permanent" : nullptr);
    }

    if (ignore.has_value()) {
        auto [count, forget] = ignore.value();
        EMotorStallDetector::Instance().SetIgnore(count, forget);
    }

    if (opt_enable_stuck || opt_enable_movement) {
        const bool enable_stuck = opt_enable_stuck.value_or(config_store().stuck_filament_detection.get());
        const bool enable_movement = opt_enable_movement.value_or(config_store().filament_movement_detection.get());
        EMotorStallDetector::Instance().SetEnabled(enable_stuck || enable_movement);
    }

    SERIAL_ECHOLNPAIR_F("Filament stuck detection ", opt_enable_stuck.value_or(config_store().stuck_filament_detection.get()) ? "on" : "off");
    SERIAL_ECHOLNPAIR_F("Filament movement detection ", opt_enable_movement.value_or(config_store().filament_movement_detection.get()) ? "on" : "off");
    SERIAL_ECHOLNPAIR_F("E-stall detector ", EMotorStallDetector::Instance().Enabled() ? "on" : "off");
}
#endif // HAS_LOADCELL()
} // anonymous namespace

/** \addtogroup G-Codes
 * @{
 */

/**### M591: Enable/Disable Filament stuck monitoring <a href="https://reprap.org/wiki/G-code#M591:_Configure_filament_monitoring">M591: Configure filament monitoring</a>
 *
 * Used by loadcell-equipped printers to pause on nozzle clogs/jams or
 * upstream filament restraint that stalls forward extrusion.
 *
 *#### Usage
 *
 *    M591 [ S | U | P | R ]
 *
 *#### Parameters
 *
 * - `S` - Enable / Disable stuck-filament detection
 * - `U` - Enable / Disable upstream filament movement detection
 * - `P` - change is permanent
 * - `R` - restore, this parameter has priority over `S` and `P` and discards them
 * - `I` - ignore this many skips before reporting (0 = report immediatelly, 1 - report the second if within the forget time)
 * - `F` - forget a skip after this time (in combination with I, in ms)
 *   Set both at once.
 *
 *   TODO: Can one parameter take two numbers?
 *
 * Without parameters prints the current state of Filament stuck monitoring, filament movement monitoring, and the shared E-stall detector.
 *
 * After the change it prints the state of both user-facing protections and the shared EMotorStallDetector onto the serial line.
 */

void PrusaGcodeSuite::M591() {
#if HAS_LOADCELL()
    std::optional<bool> enable_e_stall;
    if (parser.seen('S')) {
        enable_e_stall = parser.byteval('S') == 1;
    }
    std::optional<bool> enable_movement_detection;
    if (parser.seen('U')) {
        enable_movement_detection = parser.byteval('U') == 1;
    }
    IsPermanent is_permanent = parser.seen('P') ? IsPermanent::yes : IsPermanent::no;
    Restore restore = parser.seen('R') ? Restore::yes : Restore::no;
    std::optional<std::tuple<uint32_t, uint32_t>> ignore = std::nullopt;
    if (parser.seen('I') && parser.seen('F')) {
        ignore = std::make_tuple(parser.ulongval('I'), parser.ulongval('F'));
    }
    m591_no_parser(enable_e_stall, enable_movement_detection, is_permanent, restore, ignore);
#else
    SERIAL_ECHOLN("Filament stuck detection not supported");
#endif // HAS_LOADCELL()
}

/** @}*/

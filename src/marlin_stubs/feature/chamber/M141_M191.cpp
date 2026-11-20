
#include "M141_M191.hpp"

#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <marlin_stubs/skippable_gcode.hpp>

#include <feature/chamber/chamber.hpp>
#include <gcode/gcode_parser.hpp>
#include <module/planner.h>
#include <lcd/ultralcd.h> // Some marlin garbage dunno
#include <marlin_server.hpp>
#include <gcode/gcode.h>
#include <feature/print_status_message/print_status_message_guard.hpp>
#include <feature/safety_timer/safety_timer.hpp>
#include <serial_printing.hpp>
#include <cmath>
#include <cstdio>

using namespace buddy;

/**
 *### M141: Set chamber temperature <a href="https://reprap.org/wiki/G-code#M141:_Set_Chamber_Temperature_.28Fast.29">M141: Set Chamber Temperature (Fast)</a>
 *
 * #### Usage
 *
 *     M141 [ S ]
 *
 *#### Parameters
 *
 * - `S` - Target temperature, in degrees Celsius. 0 = no target temperature
 */

void PrusaGcodeSuite::M141() {
    using buddy::Temperature;

    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    if (const auto temp = p.option<Temperature>('S')) {
        M141_no_parser({ .target_temp = *temp });
    }
}

/**
 *
 *### M191: Wait for chamber temperature <a href="https://reprap.org/wiki/G-code#M191:_Wait_for_chamber_temperature_to_reach_target_temp">M191: Wait for chamber temperature to reach target temp</a>
 *
 * #### Usage
 *
 *     M191 [ S | R | C ]
 *
 *#### Parameters
 *
 * - `S` - Target temperature in degrees Celsius. Wait only for heating.
 * - `R` - Target temperature in degrees Celsius. Wait for both cooling and heating.
 * - `C` - Target temperature in degrees Celsius. Wait only for cooling.
 */

void PrusaGcodeSuite::M191() {
    using buddy::Temperature;

    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    if (const auto opt = p.option_multikey<Temperature>({ 'S', 'R', 'C' })) {
        M141_no_parser({
            .target_temp = opt->first,
            .wait_for_heating = (opt->second) != 'C',
            .wait_for_cooling = (opt->second) != 'S',
        });
    }
}

void PrusaGcodeSuite::M141_no_parser(const M141Args &args) {
    using buddy::Temperature;

    // Keep everything heated up while we're waiting
    buddy::SafetyTimerBlocker safety_timer_blocker;

    if (!chamber().capabilities().temperature_control_at_this_moment()) {
        SERIAL_ERROR_MSG("Chamber does not allow temperature control");
        return;
    }

    auto target = args.target_temp;
    if (target == 0) {
        chamber().set_target_temperature({});
        return;
    }

    // The temperature might have gotten cropped due to chamber limitations - make sure that we're waiting for the one that is actually set
    target = *chamber().set_target_temperature(target);
    if (!args.wait_for_cooling && !args.wait_for_heating) {
        return;
    }

    chamber().set_heating_wait_active(args.wait_for_heating);

    /// How long we should wait until displaying a warning that we're failing to reach the temperature
    static constexpr int32_t warning_timeout_ms = 30 * 60 * 1000;

    uint32_t warning_timeout_start = ticks_ms();
    SkippableGCode::Guard skippable_operation;

    PrintStatusMessageGuard statusGuard;
    SerialPrinting::notify_workflow("chamber_temperature", "open", "Waiting for chamber temperature");
    uint32_t last_report = ticks_ms();
    bool first_report = true;
    const char *wait_result = "closed";
    const char *wait_message = "Chamber temperature ready";
    while (true) {
        if (planner.draining()) {
            // We're aborting -> stop waiting
            wait_result = "cancelled";
            wait_message = "Chamber temperature wait cancelled";
            break;
        }

        const auto live_target = chamber().target_temperature();
        if (!live_target || *live_target == 0) {
            wait_message = "Chamber temperature wait disabled";
            break;
        }
        if (target != *live_target) {
            target = *live_target;
            warning_timeout_start = ticks_ms();
            first_report = true;
        }
        const auto current = chamber().current_temperature();
        static const Temperature tolerance = 3;

        statusGuard.update<PrintStatusMessage::waiting_for_chamber_temp>({ .current = current.value_or(0), .target = target });

        const auto now = ticks_ms();
        if (first_report || ticks_diff(now, last_report) >= 1000) {
            char message[96];
            if (current && std::isfinite(*current) && *current > -100 && *current < 500) {
                snprintf(message, sizeof(message), "Waiting for chamber: %d/%d C", int(std::lround(*current)), int(std::lround(target)));
            } else {
                snprintf(message, sizeof(message), "Waiting for chamber: unknown/%d C", int(std::lround(target)));
            }
            SerialPrinting::notify_progress("chamber_temperature", "active", "temperature_wait", message, -1);
            last_report = now;
            first_report = false;
        }

        // Show a heat failure warning if we're waiting for too long
        if (ticks_diff(now, warning_timeout_start) >= warning_timeout_ms && !marlin_server::is_warning_active(WarningType::FailedToReachChamberTemperature)) {
            marlin_server::set_warning(WarningType::FailedToReachChamberTemperature);
        }

        if (marlin_server::is_warning_active(WarningType::FailedToReachChamberTemperature)) {
            switch (marlin_server::get_response_from_phase(warning_type_phase(WarningType::FailedToReachChamberTemperature))) {

            case Response::Ok:
                marlin_server::clear_warning(WarningType::FailedToReachChamberTemperature);
                warning_timeout_start = now;
                break;

            case Response::Skip:
                skippable_gcode().request_skip();
                break;

            default:
                break;
            }
        }

        if (skippable_operation.is_skip_requested()) {
            // Skip requested -> exit
            wait_result = "skipped";
            wait_message = "Chamber temperature wait skipped";
            break;

        } else if (!current.has_value()) {
            // We don't know chamber temperature -> wait until we do

        } else if (std::abs(*current - target) <= tolerance) {
            // We're at the target -> done
            break;

        } else if (*current < target + tolerance && !args.wait_for_heating) {
            // We're cool and not waiting for heat -> done
            break;

        } else if (*current > target - tolerance && !args.wait_for_cooling) {
            // We're hot and not waiting for cooling -> done
            break;
        }

        idle(true);
    }

    marlin_server::clear_warning(WarningType::FailedToReachChamberTemperature);
    chamber().set_heating_wait_active(false);
    SerialPrinting::notify_workflow("chamber_temperature", wait_result, wait_message);
}

#include "controller.hpp"
#include "policy.hpp"

#include <fanctl.hpp>
#include <timing.h>
#include <Marlin/src/core/serial.h>
#include <algorithm>

namespace buddy::dock_cooldown {
namespace {
    Policy policy;

    void apply() {
        auto &fan = Fans::dock_fan();
        if (fan.get_pwm() != policy.pwm()) {
            fan.set_pwm(policy.pwm());
        }
    }
} // namespace

void configure(bool enabled, uint32_t seconds) {
    if (!policy.enabled()) {
        policy.request_pwm(Fans::dock_fan().get_pwm());
    }
    policy.configure(enabled, seconds, ticks_ms());
    apply();
}

void request_pwm(uint16_t pwm) {
    policy.request_pwm(std::min<uint16_t>(pwm, 255));
    policy.step(ticks_ms());
    apply();
}

uint8_t requested_pwm() {
    return policy.enabled() ? policy.requested_pwm() : Fans::dock_fan().get_pwm();
}

void before_park(uint8_t tool) {
    // INDX cannot measure a parked nozzle. Until a cold classification is
    // validated, every physical park is potentially hot, including failures.
    policy.hot_park(tool, ticks_ms());
    if (policy.enabled()) {
        apply(); // Before motion into the dock or nozzle release.
    }
}

void parked(uint8_t tool) {
    // Start the minimum interval at completed release, not at queued T time.
    policy.hot_park(tool, ticks_ms());
    if (policy.enabled()) {
        apply();
    }
}

void picked(uint8_t tool) {
    policy.picked(tool);
}

void step() {
    // User-selected timed cooling: parked INDX nozzles have no live readings.
    policy.step(ticks_ms());
    if (policy.enabled()) {
        apply();
    }
}

void report() {
    policy.step(ticks_ms());
    SERIAL_ECHOPGM("RME_DOCK_COOLDOWN enabled=");
    SERIAL_ECHO(policy.enabled());
    SERIAL_ECHOPGM(" seconds=");
    SERIAL_ECHO(policy.seconds());
    SERIAL_ECHOPGM(" remaining=");
    SERIAL_ECHO(policy.remaining_s());
    SERIAL_ECHOPGM(" pending_mask=");
    SERIAL_ECHO(unsigned(policy.pending()));
    SERIAL_ECHOPGM(" mode=timed pwm=");
    SERIAL_ECHOLN(unsigned(Fans::dock_fan().get_pwm()));
}
} // namespace buddy::dock_cooldown

/// @file
#pragma once

#include "PuppyModbus.hpp"

#include <atomic>
#include <freertos/mutex.hpp>
#include <optional>
#include <otp/types.hpp>
#include <xbuddy_extension/modbus.hpp>

namespace buddy::puppies {

/// Master-side driver for the XLS XL-CAN bridge puppy.
///
/// The bridge runs the xl_can variant of the xBuddy Extension firmware, so
/// its Modbus map is a subset of xBuddyExtension::modbus, reused here rather
/// than defining a bridge-specific layout.
class XlCan final {
public:
    /// Read a small register block to verify the bridge responds at its
    /// assigned Modbus address. Used by PuppyBootstrap right after the app
    /// is started, in the same role as XBuddyExtension::ping().
    CommunicationStatus ping(PuppyModbus &);

    /// Initial post-bootstrap scan; currently just ping().
    CommunicationStatus initial_scan(PuppyModbus &);

    /// Periodic refresh from the puppy task: reads the Status block and
    /// flushes the desired Config state to the bridge.
    CommunicationStatus refresh(PuppyModbus &);

    void set_otp(const OTP_v5 &);
    OTP_v5 get_otp() const;

    /// Whether the bridge was discovered during bootstrap. The xlBuddy master
    /// image is shared across plain XL and XLS; only XLS physically has the
    /// bridge, so this flag distinguishes the two at runtime. Set by the
    /// puppy task right after bootstrap completes.
    bool is_enabled() const { return enabled.load(); }
    void set_enabled(bool e) { enabled.store(e); }

    /// Updates the reset pin on the MMU port on the XL-CAN (where modular bed is connected to)
    /// Executes the modbus transcation blockingly - needed for the bootstrap
    ///
    /// CONSTRAINT: never call with nreset=true outside the bootstrap arming
    /// step — Q2 (the transistor controlling the edge network NRST on the XLCAN)
    /// disturbs MB NRST on BOTH edges, so a rising edge at runtime
    /// resets (or hangs) the running MB app. The HIGH phase must also be held
    /// long enough to arm the edge network before the falling edge; both
    /// invariants are enforced in write_dock_reset_pin (PuppyBootstrap.cpp),
    /// which is the only legitimate H/L driver of this line.
    CommunicationStatus set_modular_bed_reset(PuppyModbus &, bool nreset);

    enum class FanSelftestMode : uint8_t {
        nop_if_selftest,
        exit_selftest,
        set_selftest
    };

    /// Set the Modular Bed cooling fan duty (0-255)
    void set_fan_pwm(uint8_t pwm, FanSelftestMode selftest_mode = FanSelftestMode::nop_if_selftest);

    /// Desired MB cooling fan duty (0-255) last set via set_fan_pwm; not a
    /// measured value.
    [[nodiscard]] uint8_t get_fan_pwm() const { return fan_pwm_desired.load(); }

    /// Last fan RPM reported by the bridge; nullopt until the bridge has
    /// answered a Status read, and again after one fails. Note is_enabled()
    /// is *not* sufficient for this: it is latched from the bootloader-level
    /// probe, before any Modbus exchange has taken place.
    [[nodiscard]] std::optional<uint16_t> get_fan_rpm() const;

private:
    mutable freertos::Mutex mutex;
    std::atomic<bool> enabled { false };
    OTP_v5 otp = {};

    using Status = xbuddy_extension::modbus::Status;
    ModbusInputRegisterBlock<Status::address, Status> status;

    /// Whether `status` holds a value the bridge actually sent. Until the
    /// first successful read it is zero-default, which would otherwise be
    /// indistinguishable from a genuine "fan stopped, no fault" report.
    std::atomic<bool> valid { false };

    // Holding-register block shared with the xBE Modbus map (the bridge fw is
    // an xBE variant). Only `mmu_nreset` and the MB fan slot of `fan_pwm` are
    // exercised today; the rest of the Config struct is written as zeros each
    // time, which the bridge ignores for fields whose underlying peripheral
    // isn't populated on the bridge PCB. A bridge-specific reduced layout is
    // a possible future optimization.
    using Config = xbuddy_extension::modbus::Config;
    ModbusHoldingRegisterBlock<Config::address, Config> config;

    std::atomic<bool> mmu_nreset_desired { false };
    static_assert(std::atomic<bool>::is_always_lock_free);

    /// Desired MB fan duty, packed into Config.fan_pwm on the next
    /// refresh_holding (deferred write — fan control isn't latency-critical).
    std::atomic<uint8_t> fan_pwm_desired { 0 };

    /// While true, only selftest_set_fan_pwm() may change fan_pwm_desired.
    std::atomic<bool> fan_selftest_active { false };

    /// Fault-transition logging memory (refresh). Puppy-task-only,
    /// mutex-protected.
    bool last_fan_power_fault = false;

    /// Read the Status block and latch `valid` on the outcome. Caller must
    /// hold `mutex`.
    CommunicationStatus read_status(PuppyModbus &);

    /// Pack the desired-state atomics into the Config block and flush to the
    /// bridge. Mirrors XBuddyExtension::refresh_holding minus the fields
    /// whose desired-state shadow we don't yet maintain. Caller must hold
    /// `mutex`.
    CommunicationStatus refresh_holding(PuppyModbus &);
};

extern XlCan xl_can;

} // namespace buddy::puppies

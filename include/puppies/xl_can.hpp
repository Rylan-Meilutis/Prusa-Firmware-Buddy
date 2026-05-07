/// @file
#pragma once

#include "PuppyModbus.hpp"

#include <atomic>
#include <freertos/mutex.hpp>
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

    /// Periodic refresh from the puppy task. Stub for now -- no cached state
    /// is exposed yet, so we just re-ping to keep the bus warm.
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
    CommunicationStatus set_modular_bed_reset(PuppyModbus &, bool nreset);

private:
    mutable freertos::Mutex mutex;
    std::atomic<bool> enabled { false };
    OTP_v5 otp = {};

    using Status = xbuddy_extension::modbus::Status;
    ModbusInputRegisterBlock<Status::address, Status> status;

    // Holding-register block shared with the xBE Modbus map (the bridge fw is
    // an xBE variant). Only `mmu_nreset` is exercised today; the rest of the
    // Config struct is written as zeros each time, which the bridge ignores
    // for fields whose underlying peripheral isn't populated on the bridge
    // PCB. A bridge-specific reduced layout is a possible future optimization.
    using Config = xbuddy_extension::modbus::Config;
    ModbusHoldingRegisterBlock<Config::address, Config> config;

    std::atomic<bool> mmu_nreset_desired { false };
    static_assert(std::atomic<bool>::is_always_lock_free);

    /// Pack the desired-state atomics into the Config block and flush to the
    /// bridge. Mirrors XBuddyExtension::refresh_holding minus the fields
    /// whose desired-state shadow we don't yet maintain. Caller must hold
    /// `mutex`.
    CommunicationStatus refresh_holding(PuppyModbus &);
};

extern XlCan xl_can;

} // namespace buddy::puppies

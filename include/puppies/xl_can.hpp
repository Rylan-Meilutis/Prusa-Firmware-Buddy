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

private:
    mutable freertos::Mutex mutex;
    std::atomic<bool> enabled { false };
    OTP_v5 otp = {};

    using Status = xbuddy_extension::modbus::Status;
    ModbusInputRegisterBlock<Status::address, Status> status;
};

extern XlCan xl_can;

} // namespace buddy::puppies

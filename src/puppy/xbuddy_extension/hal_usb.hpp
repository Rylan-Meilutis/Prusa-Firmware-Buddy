/// @file
#pragma once

namespace hal::usb {

/// Initialize USB pins.
void init();

/// Control the power pin for USB.
/// Enabling pin will power any device connected up to 5V/1A (it might be just the usb basic 800mA).
void power_pin_set(bool enabled);

} // namespace hal::usb

#pragma once

#include <puppies/BootloaderProtocol.hpp>

namespace buddy::puppies {

/// Instruct XBuddy Extension's bootloader to jump into the firmware.
/// This also correctly handles missing or outdated firmware.
/// This doesn't handle crash dumps, because they are not produced.
void xbuddy_extension_bootstrap(BootloaderProtocol &);

} // namespace buddy::puppies

#pragma once

namespace buddy::puppies {

/// Entry point for task taking care of the puppies via Modbus.
void run();

/// Suspend puppy task.
void suspend_puppy_task();

} // namespace buddy::puppies

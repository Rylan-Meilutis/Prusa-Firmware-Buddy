
/// @file
/// Home for the FreeRTOS application hooks, each dispatching to a few
/// lightweight subsystem functions. vApplicationTickHook runs in the SysTick
/// ISR context, so keep everything it reaches short and non-blocking.

#include "cpu_utils.hpp"

extern "C" void vApplicationTickHook() {
    cpu_utils::compute_cpu_load();
}

extern "C" void vApplicationIdleHook() {
    cpu_utils::mark_cpu_idle();
}

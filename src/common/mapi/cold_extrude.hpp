/// @file
#pragma once

#include <feature/safety_timer/safety_timer.hpp>
#include <module/temperature.h>

namespace mapi {

/// Guard that allows cold extrusion for the duration of its existence
class ColdExtrudeGuard {

public:
#if ENABLED(PREVENT_COLD_EXTRUSION)
    ColdExtrudeGuard()
        : prev_allow_cold_extrude_(thermalManager.allow_cold_extrude) {
        thermalManager.allow_cold_extrude = true;
    }
    ~ColdExtrudeGuard() {
        thermalManager.allow_cold_extrude = prev_allow_cold_extrude_;
    }
#else
    ColdExtrudeGuard() = default;
    ~ColdExtrudeGuard() = default;
#endif

private:
    /// Do not wait for the target temperatures to be restored for moves issued in this guard
    buddy::SafetyTimerNonBlockingGuard non_blocking_safety_;

#if ENABLED(PREVENT_COLD_EXTRUSION)
    const bool prev_allow_cold_extrude_;
#endif
};

} // namespace mapi

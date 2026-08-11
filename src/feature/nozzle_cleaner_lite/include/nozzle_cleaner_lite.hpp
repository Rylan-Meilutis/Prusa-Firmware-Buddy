#pragma once

#include <option/has_nozzle_cleaner_lite.h>

#include <gcode/inject_queue_actions.hpp>
#include <optional>
#include <string_view>
#include <str_utils.hpp>

#include <core/types.h>
#include <mapi/parking.hpp>
#include <utils/badge.hpp>

class unified_bed_leveling;

namespace nozzle_cleaner_lite {

/// True when the running unit actually has a nozzle cleaner lite version enabled.
bool is_available();

/// What the clean is part of, which decides where the cleaning temperature
/// comes from and what happens to the nozzle temperature target afterwards
enum class CleanType : uint8_t {
    /// A clean of its own (G12): cleans at the loaded filament's preheat
    /// temperature, which is warm enough to soften ooze without making it
    /// flow, and the target from before cleaning is restored.
    standalone,

    /// A clean before MBL, on the tool that probes afterwards: cleans at the
    /// temperature the preceding gcode set for probing (falling back to the
    /// preheat temperature if it set none), and the touchpoint cool-down
    /// target is kept, so probing runs as hot as possible without oozing
    /// (nozzle thermal expansion).
    probing_tool,
};

/// Home if needed, probe the cleaner's touchpoint reference point, run the
/// clean cycle and, unless the tool only parks afterwards, rest on the
/// touchpoint until the nozzle cools down.
/// Caller must check is_available() first.
/// \returns true if the whole sequence completed
bool clean(CleanType clean_type);

} // namespace nozzle_cleaner_lite

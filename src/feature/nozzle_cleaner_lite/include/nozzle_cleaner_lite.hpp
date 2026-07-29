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

/// What the clean is part of, which decides what happens to the nozzle
/// temperature target afterwards
enum class CleanType : uint8_t {
    /// A clean of its own (G12): the target from before cleaning is restored.
    standalone,
    /// A clean before MBL, on the tool that probes afterwards: the touchpoint
    /// cool-down target is kept, so probing runs as hot as possible without
    /// oozing (nozzle thermal expansion).
    probing_tool,
    /// A clean before MBL, on a used tool that parks instead of probing: the
    /// touchpoint rest is skipped and the heater turned off, so the nozzle
    /// cools naturally in the dock without anyone waiting for it.
    parked_tool,
};

/// Home if needed, probe the cleaner's touchpoint reference point, run the
/// clean cycle and, unless the tool only parks afterwards, rest on the
/// touchpoint until the nozzle cools down.
/// Caller must check is_available() first.
/// \returns true if the whole sequence completed
bool clean(CleanType clean_type);

/// Clean all tools used by the current print: preheat them in parallel, then
/// tool-change to each and run the clean cycle. On success the tool that
/// probes MBL afterwards is left at its cool-down temperature and the other
/// tools are turned off to cool naturally; on failure the previous
/// temperature targets are restored. Caller must check is_available() first.
/// Only intended to run once, right before UBL's print-start probing.
void clean_before_probing(Badge<unified_bed_leveling>);

constexpr int16_t cleaning_temp_diff = 55; // The nozzle is heated to filament_temp - cleaning_temp_diff for cleaning
constexpr int16_t fallback_cleaning_temperature = 180;

} // namespace nozzle_cleaner_lite

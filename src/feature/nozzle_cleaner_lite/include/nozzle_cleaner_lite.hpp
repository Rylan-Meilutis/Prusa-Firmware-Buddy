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

/// Home if needed, probe the cleaner's touchpoint reference point, and run
/// the clean cycle. Caller must check is_available() first.
void clean();

/// Clean all tools used by the current print: preheat them in parallel, then
/// tool-change to each and run the clean cycle. Previous temperature targets
/// are restored on return. Caller must check is_available() first. Only
/// intended to run once, right before UBL's print-start probing.
void clean_before_probing(Badge<unified_bed_leveling>);

constexpr int16_t cleaning_temp_diff = 55; // The nozzle is heated to filament_temp - cleaning_temp_diff for cleaning
constexpr int16_t fallback_cleaning_temperature = 180;

} // namespace nozzle_cleaner_lite

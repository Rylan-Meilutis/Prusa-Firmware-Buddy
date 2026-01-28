/// @file
#pragma once

#include <cstdint>
#include <span>

/// Temperature table for a thermistor in a marlin format (thermistors.h)
/// First item is the raw readout from the adc
// !!! THE RAW VALUES ARE TYPICALLY PRE-MULTIPLIED BY OVERSAMPLENR
/// Second item is the temperature in degrees celsius
/// The table is ALWAYS sorted by the first item ascending
/// !!! If the thermistor is NTC, the second item is descending
using MarlinTempTable = std::span<const int16_t[2]>;

/// Looks up the temptable and returns the celsius matching with the raw_value
/// Interpolates
float marlin_temptable_lookup(MarlinTempTable temp_table, int16_t raw_value);

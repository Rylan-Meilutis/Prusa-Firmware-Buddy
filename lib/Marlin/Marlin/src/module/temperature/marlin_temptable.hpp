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

/// @returns whether the temptable is for a NTC (that is, decreasing raw value with increasing temperature)
inline bool marlin_temptable_is_ntc(MarlinTempTable temp_table) {
    return temp_table[1][1] < temp_table[0][1];
}

/// Structure for storing and checking mintemp/maxtemp on the raw level
/// The algos have unittests
struct MarlinTemptableRawMinMax {
    /// Minimum and maximum raw values
    /// !!! If the temptable is NTC, the raw > raw_min/max comparison must be inverted
    /// !!! The values are MULTIPLIED BY OVERSAMPLENR
    int16_t raw_min = 0;
    int16_t raw_max = 0;

    /// Computes the raw limits from celsius limits
    static MarlinTemptableRawMinMax compute(MarlinTempTable temp_table, int16_t mintemp_c, int16_t maxtemp_c);

    /// @returns if the sample is over maxtemp_c
    bool is_maxtemp(int16_t raw) const {
        return (raw_max > raw_min) == (raw > raw_max);
    }

    /// @returns if the sample is under mintemp_c
    bool is_mintemp(int16_t raw) const {
        return (raw_min < raw_max) == (raw < raw_min);
    }

    /// Possibly raises tempManager.min_temp_error/max_temp_error
    void check_temperror(int16_t raw_value, int8_t heater_index, bool check_mintemp);
};

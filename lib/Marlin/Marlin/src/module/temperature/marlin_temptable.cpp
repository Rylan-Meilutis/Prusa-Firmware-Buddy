/// @file
#include "marlin_temptable.hpp"

#include <module/thermistor/thermistors.h>
#include <module/temperature.h>

float marlin_temptable_lookup(MarlinTempTable temp_table, int16_t raw) {
    const auto TBL = temp_table.data();
    const auto LEN = temp_table.size();

    // Ugly code stolen from temperature.cpp
    // One day, we want to get rid of it, but it's still used on several places
    /**
     * Bisect search for the range of the 'raw' value, then interpolate
     * proportionally between the under and over values.
     */
    uint8_t l = 0,
            r = LEN, m;
    for (;;) {
        m = (l + r) >> 1;
        if (!m) {
            return TBL[0][1];
        }
        if (m == l || m == r) {
            return TBL[LEN - 1][1];
        }
        short v00 = TBL[m - 1][0],
              v10 = TBL[m - 0][0];
        if (raw < v00) {
            r = m;
        } else if (raw > v10) {
            l = m;
        } else {
            const short v01 = TBL[m - 1][1],
                        v11 = TBL[m - 0][1];
            return v01 + (raw - v00) * float(v11 - v01) / float(v10 - v00);
        }
    }
}

MarlinTemptableRawMinMax MarlinTemptableRawMinMax::compute(MarlinTempTable temp_table, int16_t mintemp_c, int16_t maxtemp_c) {
    /*
        This function is intended to replace this ugly code from temperature.cpp:

        #define _TEMP_MIN_E(NR) do{ \
            temp_range[NR].mintemp = HEATER_ ##NR## _MINTEMP; \
            while (analog_to_celsius_hotend(temp_range[NR].raw_min, NR) < HEATER_ ##NR## _MINTEMP) \
            temp_range[NR].raw_min += TEMPDIR(NR) * (OVERSAMPLENR); \
        }while(0)
        #define _TEMP_MAX_E(NR) do{ \
            temp_range[NR].maxtemp = HEATER_ ##NR## _MAXTEMP; \
            while (analog_to_celsius_hotend(temp_range[NR].raw_max, NR) > HEATER_ ##NR## _MAXTEMP) \
            temp_range[NR].raw_max -= TEMPDIR(NR) * (OVERSAMPLENR); \
        }while(0)
    */

    MarlinTemptableRawMinMax result {
        .raw_min = temp_table.front()[0],
        .raw_max = temp_table.back()[0],
    };
    int16_t step = OVERSAMPLENR;

    if (marlin_temptable_is_ntc(temp_table)) {
        std::swap(result.raw_min, result.raw_max);
        step *= -1;
    }

    while (marlin_temptable_lookup(temp_table, result.raw_min) < mintemp_c) {
        result.raw_min += step;
    }

    while (marlin_temptable_lookup(temp_table, result.raw_max) > maxtemp_c) {
        result.raw_max -= step;
    }

    return result;
}

void MarlinTemptableRawMinMax::check_temperror(int16_t raw_value, int8_t heater_index, bool check_mintemp) {
    if (is_maxtemp(raw_value)) {
        thermalManager.max_temp_error((heater_ind_t)heater_index);
    }

    if (check_mintemp && is_mintemp(raw_value)) {
        thermalManager.min_temp_error((heater_ind_t)heater_index);
    }
}

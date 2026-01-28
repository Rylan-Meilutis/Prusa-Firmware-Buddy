#pragma once

#include <cstdint>

using heater_ind_t = int8_t;

inline struct {
    void max_temp_error(heater_ind_t) {}
    void min_temp_error(heater_ind_t) {}
} thermalManager;

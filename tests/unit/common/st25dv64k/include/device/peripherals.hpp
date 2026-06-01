#pragma once

#include <cstdint>

struct I2C_HandleTypeDef {};
inline I2C_HandleTypeDef hi2c_eeprom_dummy;
constexpr I2C_HandleTypeDef *i2c_handle_eeprom = &hi2c_eeprom_dummy;
inline void i2c_init_eeprom() {}

constexpr auto HAL_MAX_DELAY = uint32_t(-1);
inline void HAL_Delay(uint32_t) {}

#pragma once

#include <cstdint>

enum class ErrCode {
    ERR_UNDEF,
    ERR_ELECTRO_I2C_TX_BUSY,
    ERR_ELECTRO_I2C_TX_ERROR,
    ERR_ELECTRO_I2C_TX_TIMEOUT,
};
inline constexpr uint8_t ERR_PRINTER_CODE = 12;

#include "hal.hpp"

#include <stm32c0xx_hal.h>

namespace hal::clk {
void enable_adc_mco() {
    // Clock for SPI-ADC peripheral
    HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_HSE, RCC_MCO2DIV_8);
}
void disable_adc_mco() {
    HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_NOCLOCK, RCC_MCO2DIV_1);
}
} // namespace hal::clk

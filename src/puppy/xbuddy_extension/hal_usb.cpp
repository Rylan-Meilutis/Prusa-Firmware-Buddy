/// @file
#include "hal_usb.hpp"

#include <stm32h5xx_hal.h>

void hal::usb::init() {
    constexpr GPIO_InitTypeDef GPIO_InitStruct {
        .Pin = GPIO_PIN_2,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
        .Alternate = 0,
    };
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void hal::usb::power_pin_set(bool enabled) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

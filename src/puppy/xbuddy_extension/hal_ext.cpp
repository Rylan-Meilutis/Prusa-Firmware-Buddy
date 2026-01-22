/// @file
#include "hal_ext.hpp"

#include <stm32h5xx_hal.h>

void hal::ext::init() {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    constexpr GPIO_InitTypeDef GPIO_InitStruct {
        .Pin = GPIO_PIN_14,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
        .Alternate = 0,
    };
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void hal::ext::set(bool state) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

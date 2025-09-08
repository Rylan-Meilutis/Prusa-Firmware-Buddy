#pragma once

#include <device/hal.h>
#include <option/can_bus_type.h>

namespace hal::peripherals {
#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
extern FDCAN_HandleTypeDef hfdcan1;
#elif CAN_BUS_TYPE_IS_UART()
    #ifdef STM32C0
extern UART_HandleTypeDef huart1;
    #elif STM32H5
extern UART_HandleTypeDef huart2;
    #else
        #error
    #endif
#else
    #error
#endif
extern SPI_HandleTypeDef hspi1;

#ifdef HASH_ALGOSELECTION_SHA256
extern HASH_HandleTypeDef hhash;
#endif
} // namespace hal::peripherals

// src/can does not expect the namespace...
using namespace hal::peripherals;

#define TIM3_OVERFLOW 1

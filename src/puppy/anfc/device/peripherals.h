#pragma once

#include <device/hal.h>

namespace hal::peripherals {
extern FDCAN_HandleTypeDef hfdcan1;
extern SPI_HandleTypeDef hspi1;

#ifdef HASH_ALGOSELECTION_SHA256
extern HASH_HandleTypeDef hhash;
#endif
} // namespace hal::peripherals

// src/can does not expect the namespace...
using namespace hal::peripherals;

#define TIM3_OVERFLOW 1

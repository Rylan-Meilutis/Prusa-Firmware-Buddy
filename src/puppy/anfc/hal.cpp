#include "hal.hpp"

#ifdef STM32H5
    #include <stm32h5xx.h>
#elifdef STM32C0
    #include <stm32c0xx.h>
#else
    #error
#endif

void hal::panic() {
    asm volatile("bkpt 0");
    NVIC_SystemReset();
}

void hal::init() {
    HAL_Init();
}

extern "C" void hal_panic() {
    hal::panic();
}

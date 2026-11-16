#include "interrupts_helper.hpp"
#include <device/peripherals.hpp>
#include <common/bsod.h>
#include <tusb.h>
#include <wdt.hpp>

extern ETH_HandleTypeDef heth;
extern HCD_HandleTypeDef hhcd_USB_OTG_HS;

// Enter CrashCatcher with the original exception frame and EXC_RETURN intact.
// Calling the normal BSOD path here uses the potentially damaged task/handler
// stack, and a subsequently triggered HardFault describes the BSOD code rather
// than the instruction that caused the original fault. CrashCatcher switches
// to its dedicated stack before calling C/C++ code and records the SCB faults.
// These must be naked tail branches: no prologue, call, or register clobber.
extern "C" __attribute__((naked)) void MemManage_Handler() {
    __asm volatile("b HardFault_Handler");
}

extern "C" __attribute__((naked)) void BusFault_Handler() {
    __asm volatile("b HardFault_Handler");
}

extern "C" __attribute__((naked)) void UsageFault_Handler() {
    __asm volatile("b HardFault_Handler");
}

extern "C" void DebugMon_Handler() {
}

TRACED_ISR(TIM8_TRG_COM_TIM14_IRQHandler, HAL_TIM_IRQHandler, &htim14);
TRACED_ISR(WWDG_IRQHandler, HAL_WWDG_IRQHandler, &hwwdg);
TRACED_ISR(OTG_FS_IRQHandler, tud_int_handler, 0);

#if !BOARD_IS_XL_DEV_KIT_XLB()

TRACED_ISR(OTG_HS_IRQHandler, HAL_HCD_IRQHandler, &hhcd_USB_OTG_HS);
TRACED_ISR(ETH_IRQHandler, HAL_ETH_IRQHandler, &heth);

#endif

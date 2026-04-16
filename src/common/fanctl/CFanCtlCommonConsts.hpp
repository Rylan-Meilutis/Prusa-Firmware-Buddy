#pragma once

#include <printers.h>
#include <option/xl_enclosure_support.h>
#include <option/has_cpu_fan.h>

// FANCTLPRINT - printing fan
inline constexpr uint8_t FANCTLPRINT_PWM_MIN = 10; // min duty cycle length 10 / 50 = 0.2 = 20%
inline constexpr uint8_t FANCTLPRINT_PWM_MAX = 50; // 1000Hz / 50 = 20Hz PWM cycle
#if PRINTER_IS_PRUSA_MK4()
inline constexpr uint16_t FANCTLPRINT_RPM_MIN = 90; // Dynamic PWM enables lower RPM
#else
inline constexpr uint16_t FANCTLPRINT_RPM_MIN = 150;
#endif
inline constexpr uint16_t FANCTLPRINT_RPM_MAX =
#if HAS_INDX()
    10000
#elif (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_iX() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL())
    6850
#elif PRINTER_IS_PRUSA_MINI()
    5000
#else
    #error "You need to specify printfans max RPM"
#endif
    ;
inline constexpr uint8_t FANCTLPRINT_PWM_THR = 20;

// On Mk3 the printer would ignore rpm measurements if the pwm was under 30%.
// Because some of the printers have a really weak print fan, it would cause
// MK3.5 users to get print fan errors on low pwm, that wouldn't happend on MK3.
// Sadly since we doing pwm differently we are not able to set it to 30% exactly,
// but rather we round to nearest int:
// <= 32% - ignore RPM measurement
// >= 33% - will trigger print fan error if the pwm is too low (FANCTLPRINT_RPM_MIN)
inline constexpr uint8_t FANCTLPRINT_MIN_PWM_TO_MEASURE_RPM =
#if PRINTER_IS_PRUSA_MK3_5()
    FANCTLPRINT_PWM_MAX * 0.3;
#else
    0;
#endif

// FANCTLHEATBREAK - heatbreak fan
inline constexpr uint8_t FANCTLHEATBREAK_PWM_MIN = 0;
inline constexpr uint8_t FANCTLHEATBREAK_PWM_MAX = 50;
inline constexpr uint16_t FANCTLHEATBREAK_RPM_MIN = 1000;
inline constexpr uint16_t FANCTLHEATBREAK_RPM_MAX =
#if HAS_INDX()
    20000
#elif (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_iX() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL())
    15180
#elif PRINTER_IS_PRUSA_MINI()
    8000
#else
    #error "You need to specify printfans max RPM"
#endif
    ;
inline constexpr uint8_t FANCTLHEATBREAK_PWM_THR = 20;
inline constexpr uint8_t FANCTLHEATBREAK_MIN_PWM_TO_MEASURE_RPM = 0;

// FANCTLENCLOSURE - enclosure fan
#if XL_ENCLOSURE_SUPPORT()
inline constexpr uint8_t FANCTLENCLOSURE_PWM_MIN = 0;
inline constexpr uint8_t FANCTLENCLOSURE_PWM_MAX = 255;
inline constexpr uint16_t FANCTLENCLOSURE_RPM_MIN = 600;
inline constexpr uint16_t FANCTLENCLOSURE_RPM_MAX = 2700;
#endif // XL_ENCLOSURE_SUPPORT

// FANCTLCPU - CPU cooling fan (XLS sandwich board)
// Driver-level thresholds used by CFanCtl3Wire / get_rpm_is_ok().
// Selftest pass/fail tolerances live separately in selftest_fans_config.hpp.
//
// Fan: LDO-D3007D04Y05X75FX, 5 V 3-wire (VCC/GND/FG, no PWM input), rated
// 10000 RPM ±15%, 4-pole motor (datasheet: BFW-8968)
// PWM drives a MOSFET on the
// sandwich board's 5 V supply. The cpu_fan_controller policy runs it at
// 0 % or 100 % only -- at 100 % duty the CFanCtlPWM output pin stays
// permanently high, so the 20 Hz soft-PWM frequency is not audible in
// practice. MIN is set below the -15 % corner with margin for part
// variance; MAX sits above the +15 % corner.
//
// XLS_TODO: validate reported RPM on real hardware. The new fan is
// 4-pole vs the previous 2-pole part; CFanCtl3Wire's tach math assumes
// 4 edges per revolution which matches a 4-pole / 2-pulse-per-rev FG.
// If on-bench RPM reads ~5000 instead of ~10000, the fan FG is actually
// 1 pulse/rev (2 edges/rev) and the tach divisor needs adjustment.
#if HAS_CPU_FAN()
inline constexpr uint8_t FANCTLCPU_PWM_MIN = 20;
inline constexpr uint8_t FANCTLCPU_PWM_MAX = 100;
inline constexpr uint8_t FANCTLCPU_PWM_THR = 70; // 3.5V is the minimum startup voltage, which is 70% of 5V
inline constexpr uint8_t FANCTLCPU_MIN_PWM_TO_MEASURE_RPM = 0;
inline constexpr uint16_t FANCTLCPU_RPM_MIN = 6375; // 7500 RPM - 15% = 6375 RPM
inline constexpr uint16_t FANCTLCPU_RPM_MAX = 8625; // 7500 RPM + 15% = 8625 RPM
#endif // HAS_CPU_FAN

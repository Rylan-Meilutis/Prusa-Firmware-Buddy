/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * temperature.cpp - temperature control
 */

#include "temperature.h"
#include "endstops.h"
#include "safe_state.h"

#include "thermistor/thermistor_5.h" // for user-space recognition of XBuddy with older LoveBoard

#include "../Marlin.h"
#include "../lcd/ultralcd.h"
#include "../core/language.h"
#include "../HAL/shared/Delay.h"
#include "bsod.h"
#include "logging/log.hpp"
#include "metric.h"
#include "../../../../src/common/hwio.h"
#include <stdint.h>
#include <device/board.h>
#include "printers.h"
#include "MarlinPin.h"
#include <module/motion.h>
#include "../../../../src/common/adc.hpp"
#include "../marlin_stubs/skippable_gcode.hpp"
#include <module/temperature/steady_state_hotend.hpp>
#include <module/temperature/temp_defines.hpp>
#include <module/temperature/heater_watch.hpp>
#include <module/temperature/marlin_temptable.hpp>
#include <module/temperature/heater_watch.hpp>
#include <option/has_power_panic.h>
#if HAS_POWER_PANIC()
  #include <power_panic.hpp>
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
  #include <Marlin/src/module/prusa/toolchanger.h>
#endif

#include <option/board_is_master_board.h>
#if BOARD_IS_MASTER_BOARD()
#include <marlin_server.hpp>
#endif

#include <option/has_planner.h>
#if HAS_PLANNER()
  #include "planner.h"
#endif

#include <feature/print_status_message/print_status_message_guard.hpp>
#include <i18n.h>
#include <option/has_chamber_api.h>
#if HAS_CHAMBER_API()
#include "feature/chamber/chamber.hpp"
#endif

#if EITHER(BABYSTEPPING, PID_EXTRUSION_SCALING)
  #include "stepper.h"
#endif

#if ENABLED(BABYSTEPPING)
  #include "../feature/babystep.h"
  #if ENABLED(BABYSTEP_ALWAYS_AVAILABLE)
    // #error dead code found by automatic analyses (see BFW-5461)
    #include "../gcode/gcode.h"
  #endif
#endif

#include "printcounter.h"

#if ENABLED(EMERGENCY_PARSER)
  // #error dead code found by automatic analyses (see BFW-5461)
  #include "../feature/emergency_parser.h"
#endif

#if ENABLED(SINGLENOZZLE)
  #include "tool_change.h"
#endif

#include <option/board_is_master_board.h>
#if BOARD_IS_MASTER_BOARD()
  #include <feature/safety_timer/safety_timer.hpp>
#endif

#include <option/has_ac_controller.h>
#include <option/has_dwarf.h>
#include <option/has_local_bed.h>
#include <option/has_remote_bed.h>
#include <option/has_modular_bed.h>
#include <option/has_toolchanger.h>
#include <utils/serial_logging_disabler.hpp>
#include <raii/scope_guard.hpp>
#include <tool/hotend/hotend.hpp>

#if HAS_AC_CONTROLLER()
    #include <puppies/ac_controller.hpp>
#endif

#include <option/has_indx.h>
#if HAS_INDX()
    #include <puppies/INDX.hpp>
#endif

LOG_COMPONENT_REF(MarlinServer);

// Rough estimate of room temperature
static constexpr float room_temperature = 25.0f;

Temperature thermalManager;

/**
 * Macros to include the heater id in temp errors. The compiler's dead-code
 * elimination should (hopefully) optimize out the unused strings.
 */

#if HAS_HEATED_BED
  #define _BED_PSTR(h) (h) == H_BED ? GET_TEXT(MSG_BED) :
#else
  #define _BED_PSTR(h)
#endif
#if HAS_TEMP_HEATBREAK_CONTROL
  #define _HEATBREAK_PSTR(h,N) ((HOTENDS) > N && (H_HEATBREAK_FIRST + h) == N) ? GET_TEXT(MSG_HEATBREAK) :
#else
  #define _HEATBREAK_PSTR(h,N)
#endif
#define _E_PSTR(h,N) ((HOTENDS) > N && (h) == N) ? PSTR(LCD_STR_E##N) :
#define HEATER_PSTR(h) _BED_PSTR(h) _E_PSTR(h,1) _E_PSTR(h,2) _E_PSTR(h,3) _E_PSTR(h,4) _E_PSTR(h,5) _HEATBREAK_PSTR(h,0) _HEATBREAK_PSTR(h,1) _HEATBREAK_PSTR(h,2) _HEATBREAK_PSTR(h,3) _HEATBREAK_PSTR(h,4) _HEATBREAK_PSTR(h,5) PSTR(LCD_STR_E0)

#define MIN_BED_POWER 0
#define MAX_BED_POWER 255

// public:

#if FAN_COUNT > 0

  std::array<uint8_t, FAN_COUNT> Temperature::fan_speed {};
  std::array<uint8_t, FAN_COUNT> Temperature::applied_fan_speed {};

  uint16_t Temperature::get_fan_speed(const uint8_t target) {
    return target < FAN_COUNT ? fan_speed[target] : 0;
  }
  /**
   * Set the print fan speed for a target extruder
   * @note You need to call apply_fan_speeds() either from planner or elsewhere to actually use the configured fan speed.
   * Set the print fan speed for a target FAN
   * !!! NOT EXTRUDER !!! THERMAL MANAGER DOES NOT WORK WITH NON-ACTIVE EXTRUDER FANS
   * See BFW-6365
   */
  void Temperature::set_fan_speed(uint8_t target, uint16_t speed) {

    NOMORE(speed, 255U);

    // @@TODO hotfix for driving of the front fan (index 1) even with the MMU code
    // It is yet unknown if there are any side effects of commenting out this piece of code.
    // The singlenozzle_fan_speed is only used in tool_change and only in a part, which is not compiled
    // in our configuration.
//    #if ENABLED(SINGLENOZZLE)
//      if (target != active_extruder) {
//        if (target < EXTRUDERS) singlenozzle_fan_speed[target] = speed;
//        return;
//      }
//      target = 0; // Always use fan index 0 with SINGLENOZZLE
//    #endif

    if (target >= FAN_COUNT) return;

    fan_speed[target] = speed;
  }

#endif // FAN_COUNT > 0

#if HAS_TEMP_BOARD
  board_info_t Temperature::temp_board; // = { 0 }

#endif

#if HAS_HEATED_BED
  bed_info_t Temperature::temp_bed; // = { 0 }
  float Temperature::bed_frame_est_celsius = TempInfo::celsius_uninitialized;
  uint32_t Temperature::bed_frame_millis = 0;

  // Init min and max temp with extreme values to prevent false errors during startup
  static MarlinTemptableRawMinMax minmaxtemp_raw_BED;

  #if WATCH_BED
  static constexpr HeaterWatch::Config watch_bed_config {
    .temp_increase = WATCH_BED_TEMP_INCREASE,
    .period_s = WATCH_BED_TEMP_PERIOD,
    .min_temp_diff = WATCH_BED_TEMP_INCREASE + TEMP_BED_HYSTERESIS + 1,
    .error_code = ErrCode::ERR_TEMPERATURE_BED_PREHEAT_ERROR,
  };

  HeaterWatch watch_bed { watch_bed_config };
  #endif
  #if DISABLED(PIDTEMPBED)
    millis_t Temperature::next_bed_check_ms;
  #endif
#endif // HAS_HEATED_BED

// Initialized by settings.load()
#if PRINTER_IS_PRUSA_iX()
  TempInfo Temperature::temp_psu;
  TempInfo Temperature::temp_ambient;
#endif

#if ENABLED(PREVENT_COLD_EXTRUSION)
  bool Temperature::allow_cold_extrude = false;
  int16_t Temperature::extrude_min_temp = EXTRUDE_MINTEMP;
#endif

// private:

#if EARLY_WATCHDOG
  // #error dead code found by automatic analyses (see BFW-5461)
  bool Temperature::inited = false;
#endif

volatile bool Temperature::temp_meas_ready = false;

#if ENABLED(PID_EXTRUSION_SCALING)
  bool Temperature::extrusion_scaling_enabled = true;
#endif

// public:

#if HAS_PID_HEATING
  #include <pid_autotune_status.hpp>

  inline void say_default_() { SERIAL_ECHOPGM("#define DEFAULT_"); }

  #if ENABLED(PID_AUTOTUNE)
  /**
   * PID Autotuning (M303)
   *
   * Alternately heat and cool the nozzle, observing its behavior to
   * determine the best PID values to achieve a stable temperature.
   * Needs sufficient heater power to make some overshoot at target
   * temperature to succeed.
   *
   * @todo Control heatbreak fan when tuning nozzle heater
   */
  void Temperature::PID_autotune(const float &target, const heater_ind_t heater, const int8_t ncycles, const bool set_result/*=false*/) {
    float current_temp = 0.0;
    int cycles = 0;
    bool heating = true;

    millis_t next_temp_ms = millis(), t1 = next_temp_ms, t2 = next_temp_ms;
    long t_high = 0, t_low = 0;

    long bias, d;
    PID_t tune_pid = { 0, 0, 0 };
    float maxT = 0, minT = 10000;

    const bool isbed = (heater == H_BED);
    bool tune_success = false;

    #if HAS_PID_FOR_BOTH
      #define GHV(B,H) (isbed ? (B) : (H))
      #if ENABLED(HW_PWM_HEATERS)
        // Need to write soft_pwm_amount even when using hardware pwm heater to prevent
        // power manager from shutting us down, leading to temperature check failure.
        #define SHV(B,H) do {                         \
          if (isbed) {                                \
            analogWrite_HEATER_BED(B);                \
            temp_bed.soft_pwm_amount = B;             \
          } else {                                    \
            analogWrite(HEATER_0_PIN, H);             \
            temp_hotend[heater].soft_pwm_amount = H;  \
          }                                           \
        } while (0)
      #else
        #define SHV(B,H) do{ if (isbed) temp_bed.soft_pwm_amount = B; else temp_hotend[heater].soft_pwm_amount = H; }while(0)
      #endif
      #define ONHEATINGSTART() (isbed ? printerEventLEDs.onBedHeatingStart() : printerEventLEDs.onHotendHeatingStart())
      #define ONHEATING(S,C,T) (isbed ? printerEventLEDs.onBedHeating(S,C,T) : printerEventLEDs.onHotendHeating(S,C,T))
    #elif ENABLED(PIDTEMPBED)
      #define GHV(B,H) B
      #if ENABLED(HW_PWM_HEATERS)
        #define SHV(B,H) analogWrite_HEATER_BED(B)
      #else
        #define SHV(B,H) (temp_bed.soft_pwm_amount = B)
      #endif
      #define ONHEATINGSTART() printerEventLEDs.onBedHeatingStart()
      #define ONHEATING(S,C,T) printerEventLEDs.onBedHeating(S,C,T)
    #else /* ENABLED(PIDTEMP) && DISABLED(PIDTEMPBED) */
      #define GHV(B,H) H
      #if ENABLED(HW_PWM_HEATERS)
        // Need to write soft_pwm_amount even when using hardware pwm heater to prevent
        // power manager from shutting us down, leading to temperature check failure.
        #define SHV(B,H) do {                         \
            analogWrite(HEATER_0_PIN, H);             \
            temp_hotend[heater].soft_pwm_amount = H;  \
        } while (0)
      #else
        #define SHV(B,H) (temp_hotend[heater].soft_pwm_amount = H)
      #endif
      #define ONHEATINGSTART() printerEventLEDs.onHotendHeatingStart()
      #define ONHEATING(S,C,T) printerEventLEDs.onHotendHeating(S,C,T)
    #endif

    #if WATCH_BED || WATCH_HOTENDS
      #define HAS_TP_BED BOTH(THERMAL_PROTECTION_BED, PIDTEMPBED)
      #if HAS_TP_BED && BOTH(THERMAL_PROTECTION_HOTENDS, PIDTEMP)
        #define GTV(B,H) (isbed ? (B) : (H))
      #elif HAS_TP_BED
        #define GTV(B,H) (B)
      #else
        #define GTV(B,H) (H)
      #endif
      const uint16_t watch_temp_period = GTV(WATCH_BED_TEMP_PERIOD, WATCH_TEMP_PERIOD);
      const uint8_t watch_temp_increase = GTV(WATCH_BED_TEMP_INCREASE, WATCH_TEMP_INCREASE);
      const float watch_temp_target = target - float(watch_temp_increase + GTV(TEMP_BED_HYSTERESIS, TEMP_HYSTERESIS) + 1);
      millis_t temp_change_ms = next_temp_ms + watch_temp_period * 1000UL;
      float next_watch_temp = 0.0;
      bool heated = false;
    #endif

    #if HAS_AUTO_FAN
      next_auto_fan_check_ms = next_temp_ms + 2500UL;
    #endif

    if (target > GHV(BED_MAXTEMP - BED_MAXTEMP_SAFETY_MARGIN, temp_range[heater].maxtemp - HEATER_MAXTEMP_SAFETY_MARGIN)) {
      SERIAL_ECHOLNPGM(MSG_PID_TEMP_TOO_HIGH);
      return;
    }

    pid_autotune_status::start(
      isbed ? pid_autotune_status::Heater::bed : pid_autotune_status::Heater::hotend,
      target,
      ncycles
    );

    SERIAL_ECHOLNPGM(MSG_PID_AUTOTUNE_START);

    disable_all_heaters();
    TERN_(AUTO_POWER_CONTROL, powerManager.power_on());

    SHV(bias = d = (MAX_BED_POWER) >> soft_pwm_bit_shift, bias = d = (PID_MAX) >> soft_pwm_bit_shift);

    wait_for_heatup = true; // Can be interrupted with M108

    #if ENABLED(NO_FAN_SLOWING_IN_PID_TUNING)
      adaptive_fan_slowing = false;
    #endif

    // PID Tuning loop
    while (wait_for_heatup) {

      const millis_t ms = millis();

      if (temp_meas_ready) { // temp sample ready
        updateTemperaturesFromRawValues();

        // Get the current temperature and constrain it
        current_temp = GHV(temp_bed.celsius, temp_hotend[heater].celsius);
        pid_autotune_status::update(current_temp, cycles, heating);
        NOLESS(maxT, current_temp);
        NOMORE(minT, current_temp);

        #if HAS_AUTO_FAN
          if (ELAPSED(ms, next_auto_fan_check_ms)) {
            checkExtruderAutoFans();
            next_auto_fan_check_ms = ms + 2500UL;
          }
        #endif

        if (heating && current_temp > target) {
          if (ELAPSED(ms, t2 + 5000UL)) {
            heating = false;
            SHV((bias - d) >> soft_pwm_bit_shift, (bias - d) >> soft_pwm_bit_shift);
            t1 = ms;
            t_high = t1 - t2;
            maxT = target;
          }
        }

        if (!heating && current_temp < target) {
          if (ELAPSED(ms, t1 + 5000UL)) {
            heating = true;
            t2 = ms;
            t_low = t2 - t1;
            if (cycles > 0) {
              const long max_pow = GHV(MAX_BED_POWER, PID_MAX);
              float last_bias = bias;
              float last_d = d;
              bias += (d * (t_high - t_low)) / (t_low + t_high);
              LIMIT(bias, 20, max_pow - 20);
              float delta_bias = bias - last_bias;
              if(delta_bias > 10) bias = last_bias + 10;
              if(delta_bias < -10) bias = last_bias - 10;
              d = (bias > max_pow >> 1) ? max_pow - 1 - bias : bias;
              float delta_d = d - last_d;
              if(delta_d > 10) d = last_d + 10;
              if(delta_d < -10) d = last_d - 10;

              SERIAL_ECHOPAIR(MSG_BIAS, bias, MSG_D, d, MSG_T_MIN, minT, MSG_T_MAX, maxT);
              if (cycles > 2) {
                const float Ku = (4.0f * d) / (float(M_PI) * (maxT - minT) * 0.5f),
                            Tu = float(t_low + t_high) * 0.001f,
                            pf = isbed ? 0.2f : 0.6f,
                            df = isbed ? 1.0f / 3.0f : 1.0f / 8.0f;

                SERIAL_ECHOPAIR(MSG_KU, Ku, MSG_TU, Tu);
                if (isbed) { // Do not remove this otherwise PID autotune won't work right for the bed!
                  tune_pid.Kp = Ku * 0.2f;
                  tune_pid.Ki = 2 * tune_pid.Kp / Tu;
                  tune_pid.Kd = tune_pid.Kp * Tu / 3;
                  SERIAL_ECHOLNPGM("\n" " No overshoot"); // Works far better for the bed. Classic and some have bad ringing.
                  SERIAL_ECHOLNPAIR(MSG_KP, tune_pid.Kp, MSG_KI, tune_pid.Ki, MSG_KD, tune_pid.Kd);
                }
                else {
                  tune_pid.Kp = Ku * pf;
                  tune_pid.Kd = tune_pid.Kp * Tu * df;
                  tune_pid.Ki = 2 * tune_pid.Kp / Tu;
                  SERIAL_ECHOLNPGM("\n" MSG_CLASSIC_PID);
                  SERIAL_ECHOLNPAIR(MSG_KP, tune_pid.Kp, MSG_KI, tune_pid.Ki, MSG_KD, tune_pid.Kd);
                }

                /**
                tune_pid.Kp = 0.33 * Ku;
                tune_pid.Ki = tune_pid.Kp / Tu;
                tune_pid.Kd = tune_pid.Kp * Tu / 3;
                SERIAL_ECHOLNPGM(" Some overshoot");
                SERIAL_ECHOLNPAIR(" Kp: ", tune_pid.Kp, " Ki: ", tune_pid.Ki, " Kd: ", tune_pid.Kd, " No overshoot");
                tune_pid.Kp = 0.2 * Ku;
                tune_pid.Ki = 2 * tune_pid.Kp / Tu;
                tune_pid.Kd = tune_pid.Kp * Tu / 3;
                SERIAL_ECHOPAIR(" Kp: ", tune_pid.Kp, " Ki: ", tune_pid.Ki, " Kd: ", tune_pid.Kd);
                */
              }
            }
            SHV((bias + d) >> soft_pwm_bit_shift, (bias + d) >> soft_pwm_bit_shift);
            cycles++;
            minT = target;
          }
        }
      }

      // Did the temperature overshoot very far?
      #ifndef MAX_OVERSHOOT_PID_AUTOTUNE
        #define MAX_OVERSHOOT_PID_AUTOTUNE 30
      #endif
      if (current_temp > target + MAX_OVERSHOOT_PID_AUTOTUNE) {
        SERIAL_ECHOLNPGM(MSG_PID_TEMP_TOO_HIGH);
        break;
      }

      // Report heater states every 2 seconds
      if (ELAPSED(ms, next_temp_ms)) {
        // Do not log heater states, only print to serial
        SerialLoggingDisabler sld;

        #if HAS_TEMP_SENSOR
          print_heater_states(isbed ? active_extruder : heater);
          SERIAL_EOL();
        #endif
        next_temp_ms = ms + 2000UL;

        // Make sure heating is actually working
        #if WATCH_BED || WATCH_HOTENDS
          if (
            #if WATCH_BED && WATCH_HOTENDS
              true
            #elif WATCH_HOTENDS
              !isbed
            #else
              isbed
            #endif
          ) {
            if (!heated) {                                          // If not yet reached target...
              if (current_temp > next_watch_temp) {                      // Over the watch temp?
                next_watch_temp = current_temp + watch_temp_increase;    // - set the next temp to watch for
                temp_change_ms = ms + watch_temp_period * 1000UL;   // - move the expiration timer up
                if (current_temp > watch_temp_target) heated = true;     // - Flag if target temperature reached
              }
              else if (ELAPSED(ms, temp_change_ms))                 // Watch timer expired
                _temp_error(heater, PSTR(MSG_T_HEATING_FAILED), GET_TEXT(MSG_HEATING_FAILED_LCD));
            }
            else if (current_temp < target - (MAX_OVERSHOOT_PID_AUTOTUNE)) // Heated, then temperature fell too far?
              _temp_error(heater, PSTR(MSG_T_THERMAL_RUNAWAY), GET_TEXT(MSG_THERMAL_RUNAWAY));
          }
        #endif
      } // every 2 seconds

      // Timeout after MAX_CYCLE_TIME_PID_AUTOTUNE minutes since the last undershoot/overshoot cycle
      #ifndef MAX_CYCLE_TIME_PID_AUTOTUNE
        #define MAX_CYCLE_TIME_PID_AUTOTUNE 20L
      #endif
      if (((ms - t1) + (ms - t2)) > (MAX_CYCLE_TIME_PID_AUTOTUNE * 60L * 1000L)) {
        SERIAL_ECHOLNPGM(MSG_PID_TIMEOUT);
        break;
      }

      if (cycles > ncycles && cycles > 2) {
        SERIAL_ECHOLNPGM(MSG_PID_AUTOTUNE_FINISHED);
        tune_success = true;

        #if HAS_PID_FOR_BOTH
          const char * const estring = GHV(PSTR("bed"), PSTR(""));
          say_default_(); serialprintPGM(estring); SERIAL_ECHOLNPAIR("Kp ", tune_pid.Kp);
          say_default_(); serialprintPGM(estring); SERIAL_ECHOLNPAIR("Ki ", tune_pid.Ki);
          say_default_(); serialprintPGM(estring); SERIAL_ECHOLNPAIR("Kd ", tune_pid.Kd);
        #elif ENABLED(PIDTEMP)
          say_default_(); SERIAL_ECHOLNPAIR("Kp ", tune_pid.Kp);
          say_default_(); SERIAL_ECHOLNPAIR("Ki ", tune_pid.Ki);
          say_default_(); SERIAL_ECHOLNPAIR("Kd ", tune_pid.Kd);
        #else
          say_default_(); SERIAL_ECHOLNPAIR("bedKp ", tune_pid.Kp);
          say_default_(); SERIAL_ECHOLNPAIR("bedKi ", tune_pid.Ki);
          say_default_(); SERIAL_ECHOLNPAIR("bedKd ", tune_pid.Kd);
        #endif

        #define _SET_BED_PID() do { \
          temp_bed.pid.Kp = tune_pid.Kp; \
          temp_bed.pid.Ki = scalePID_i(tune_pid.Ki); \
          temp_bed.pid.Kd = scalePID_d(tune_pid.Kd); \
        }while(0)

        #define _SET_EXTRUDER_PID() do { \
          PID_PARAM(Kp, heater) = tune_pid.Kp; \
          PID_PARAM(Ki, heater) = scalePID_i(tune_pid.Ki); \
          PID_PARAM(Kd, heater) = scalePID_d(tune_pid.Kd); \
          updatePID(); }while(0)

        // Use the result? (As with "M303 U1")
        if (set_result) {
          #if HAS_PID_FOR_BOTH
            if (isbed) _SET_BED_PID(); else _SET_EXTRUDER_PID();
          #elif ENABLED(PIDTEMP)
            _SET_EXTRUDER_PID();
          #else
            _SET_BED_PID();
          #endif
        }

        goto EXIT_M303;
      }
      ui.update();
    }

    disable_all_heaters();

    EXIT_M303:
      pid_autotune_status::finish(tune_success, current_temp, tune_pid.Kp, tune_pid.Ki, tune_pid.Kd);
      #if ENABLED(NO_FAN_SLOWING_IN_PID_TUNING)
        adaptive_fan_slowing = true;
      #endif
      return;
  }
  #endif

#endif // HAS_PID_HEATING

/**
 * Class and Instance Methods
 */

int16_t Temperature::getHeaterPower(const heater_ind_t heater_id) {
  #if HAS_HEATED_BED
    if (heater_id == H_BED) {
      #if HAS_REMOTE_BED()
        return 0;
      #else
        return temp_bed.soft_pwm_amount;
      #endif
    }
  #endif
  if (heater_id >= H_NOZZLE_FIRST && heater_id <= H_NOZZLE_LAST) {
    const uint8_t tool_id = heater_id - (uint8_t)H_NOZZLE_FIRST;
    return Hotend::for_tool(tool_id).nozzle_heater_pwm().value;
  }
  #if HAS_TEMP_HEATBREAK
    if (heater_id >= H_HEATBREAK_FIRST && heater_id <= H_HEATBREAK_LAST) {
      const uint8_t tool_id = heater_id - (uint8_t)H_HEATBREAK_FIRST;
      return Hotend::for_tool(tool_id).heatbreak_fan_pwm().value;
    }
  #endif //HAS_TEMP_HEATBREAK

  return 0;
}

//
// Temperature Error Handlers
//

inline void loud_kill(PGM_P const lcd_msg, const heater_ind_t heater) {
  kill(lcd_msg, HEATER_PSTR(heater));
}

void Temperature::_temp_error(const heater_ind_t heater, PGM_P const serial_msg, PGM_P const lcd_msg) {
  SERIAL_ERROR_START();
  serialprintPGM(serial_msg);
  SERIAL_ECHOPGM(MSG_STOPPED_HEATER);
  if (heater >= 0) SERIAL_ECHO((int)heater);
  else SERIAL_ECHOPGM(MSG_HEATER_BED);
  SERIAL_EOL();

  // Disable the local heaters in an ISR-safe way
  buddy_disable_heaters();

  loud_kill(lcd_msg, heater);
}

void Temperature::max_temp_error(const heater_ind_t heater) {
  #if HAS_HEATED_BED
    if (H_BED == heater) {
      _temp_error(heater, PSTR(MSG_T_MAXTEMP), GET_TEXT(MSG_ERR_MAXTEMP_BED));
      return;
    }
  #endif
  #if HAS_TEMP_HEATBREAK
    //we have multiple heartbreak thermistors and they have always the highest ID
    if(heater >= H_HEATBREAK_FIRST){
        _temp_error(heater, PSTR(MSG_T_MAXTEMP), GET_TEXT(MSG_ERR_MAXTEMP_HEATBREAK));
    }
  #endif
  _temp_error(heater, PSTR(MSG_T_MAXTEMP), GET_TEXT(MSG_ERR_MAXTEMP));
}

void Temperature::min_temp_error(const heater_ind_t heater) {
  #if HAS_HEATED_BED
    if (H_BED == heater) {
      _temp_error(heater, PSTR(MSG_T_MINTEMP), GET_TEXT(MSG_ERR_MINTEMP_BED));
      return;
    }
  #endif
  #if HAS_TEMP_HEATBREAK
    //we have multiple heartbreak thermistors and they have always the highest ID
    if(heater >= H_HEATBREAK_FIRST){
        _temp_error(heater, PSTR(MSG_T_MINTEMP), GET_TEXT(MSG_ERR_MINTEMP_HEATBREAK));
    }
  #endif
  _temp_error(heater, PSTR(MSG_T_MINTEMP), GET_TEXT(MSG_ERR_MINTEMP));
}

#if ENABLED(PIDTEMPBED)

  float Temperature::get_pid_output_bed() {

    #if DISABLED(PID_OPENLOOP)

      static PID_t work_pid{0};
      static float temp_iState = 0, temp_dState = 0;
      static bool pid_reset = true;
      float pid_output = 0;
      const float pid_error = temp_bed.target - temp_bed.celsius;

      if (!temp_bed.target || pid_error < -(PID_FUNCTIONAL_RANGE)) {
        pid_output = 0;
        pid_reset = true;
      }
      else if (pid_error > PID_FUNCTIONAL_RANGE) {
        pid_output = MAX_BED_POWER;
        pid_reset = true;
      }
      else {
        if (pid_reset) {
          temp_iState = 0.0;
          work_pid.Kd = 0.0;
          pid_reset = false;
        }

        work_pid.Kp = temp_bed.pid.Kp * pid_error;
        work_pid.Kd = work_pid.Kd + PID_K2 * (temp_bed.pid.Kd * (pid_error - temp_dState) - work_pid.Kd);

        pid_output = work_pid.Kp + work_pid.Kd + float(MIN_BED_POWER);

        //Sum error only if it has effect on output value
        if (!((((pid_output + work_pid.Ki) < 0) && (pid_error < 0))
           || (((pid_output + work_pid.Ki) > MAX_BED_POWER) && (pid_error > 0 )))) {
          temp_iState += pid_error;
        }
        work_pid.Ki = temp_bed.pid.Ki * temp_iState;

        pid_output += work_pid.Ki;
        temp_dState = pid_error;

        pid_output = std::clamp<float>(pid_output, 0, MAX_BED_POWER); //TODO shouldn't be low limit MIN_BED_POWER?
      }

    #else // PID_OPENLOOP
      // #error dead code found by automatic analyses (see BFW-5461)

      const float pid_output = std::clamp<float>(temp_bed.target, 0, MAX_BED_POWER);

    #endif // PID_OPENLOOP

    #if ENABLED(PID_BED_DEBUG)
      // #error dead code found by automatic analyses (see BFW-5461)
    {
      SERIAL_ECHO_START();
      SERIAL_ECHOLNPAIR(
        " PID_BED_DEBUG : Input ", temp_bed.celsius, " Output ", pid_output,
        #if DISABLED(PID_OPENLOOP)
          // #error dead code found by automatic analyses (see BFW-5461)
          MSG_PID_DEBUG_PTERM, work_pid.Kp,
          MSG_PID_DEBUG_ITERM, work_pid.Ki,
          MSG_PID_DEBUG_DTERM, work_pid.Kd,
        #endif
      );
    }
    #endif

    return pid_output;
  }

#endif // PIDTEMPBED

/**
 * Manage heating activities for extruder hot-ends and a heated bed
 *  - Acquire updated temperature readings
 *    - Also resets the watchdog timer
 *  - Invoke thermal runaway protection
 *  - Manage extruder auto-fan
 *  - Apply filament width to the extrusion rate (may move)
 *  - Update the heated bed PID output value
 *  - Kickstart fans
 */
void Temperature::manage_heater() {

  #if EARLY_WATCHDOG
    // #error dead code found by automatic analyses (see BFW-5461)
    // If thermal manager is still not running, make sure to at least reset the watchdog!
    if (!inited) return watchdog_refresh();
  #endif

  #if ENABLED(EMERGENCY_PARSER)
    // #error dead code found by automatic analyses (see BFW-5461)
    if (emergency_parser.killed_by_M112) kill();
  #endif

  #if HAS_POWER_PANIC()
    if (power_panic::ac_fault_triggered) {
      disable_all_heaters();
    }
  #endif

  // !!! This is SURPRISINGLY EXTREMELY IMPORTANT
  // It limits the manage heater stepping to ~TEMP_TIMER_FREQUENCY
  // which somewhat ensures constant-ish dT for the PID regulators
  // without this, things would go haywire
  // This is still very sloppy though - BFW-8354
  if (!temp_meas_ready) return;

  updateTemperaturesFromRawValues(); // also resets the watchdog

  millis_t ms = millis();

  for (auto tool : PhysicalToolIndex::all()) {
    Hotend::for_tool(tool).manage();
  }

  #if HAS_HEATED_BED

    #if ENABLED(THERMAL_PROTECTION_BED)
      if (degBed() > BED_MAXTEMP)
        _temp_error(H_BED, PSTR(MSG_T_THERMAL_RUNAWAY), GET_TEXT(MSG_THERMAL_RUNAWAY_BED));
    #endif

    #if WATCH_BED
      watch_bed.update(degBed());
    #endif // WATCH_BED

    do {

      #if DISABLED(PIDTEMPBED)
        if (PENDING(ms, next_bed_check_ms)
        ) break;
        next_bed_check_ms = ms + 5000; // ms between checks in bang-bang control
      #endif

      #if HAS_THERMALLY_PROTECTED_BED
        thermal_runaway_bed.step(temp_bed.celsius, temp_bed.target, H_BED, THERMAL_PROTECTION_BED_PERIOD, THERMAL_PROTECTION_BED_HYSTERESIS);
      #endif

      {
        #if ENABLED(PIDTEMPBED)
          temp_bed.soft_pwm_amount = WITHIN(temp_bed.celsius, BED_MINTEMP, BED_MAXTEMP) ? (int)get_pid_output_bed() : 0;
        #else
          // Check if temperature is within the correct band
          if (WITHIN(temp_bed.celsius, BED_MINTEMP, BED_MAXTEMP)) {
              temp_bed.soft_pwm_amount = temp_bed.celsius < temp_bed.target ? MAX_BED_POWER : 0;
          }
          else {
            temp_bed.soft_pwm_amount = 0;
          }
        #endif
      }

    } while (false);

  #endif

  UNUSED(ms);
  #if HAS_LOCAL_BED()
    analogWrite_HEATER_BED(temp_bed.soft_pwm_amount);
  #endif

  manage_fans();
}

void Temperature::manage_fans() {
  #if HAS_POWER_PANIC()
    if(power_panic::ac_fault_triggered) {
      // Override anything any gcode might have ever set
      applied_fan_speed.fill(0);
    }
  #endif

  #if HAS_FAN0
    analogWrite(FAN0_PIN, applied_fan_speed[0]);
  #endif
  #if HAS_FAN1
    analogWrite(FAN1_PIN, applied_fan_speed[1]);
  #endif
  #if HAS_FAN2
    // #error dead code found by automatic analyses (see BFW-5461)
    analogWrite(FAN2_PIN, applied_fan_speed[2]);
  #endif
}

static bool temperatures_ready_state = false;

bool Temperature::temperatures_ready() {
  return temperatures_ready_state;
}

bool Temperature::are_all_temperatures_reached() {
  #if HAS_TEMP_HOTEND
    if(!are_hotend_temperatures_reached()) {
      return false;
    }
  #endif

  #if HAS_HEATED_BED
    if (!is_bed_temperature_reached()) {
      return false;
    }
  #endif

  return true;
}

#if HAS_HEATED_BED

#if PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE()
constexpr float compensate_bed_temperature(float celsius) {
  float _offset = 10;
  float _offset_center = 50;
  float _offset_start = 40;
  float _first_koef = (_offset / 2) / (_offset_center - _offset_start);
  float _second_koef = (_offset / 2) / (100 - _offset_center);

  if (celsius >= _offset_start && celsius <= _offset_center) {
      celsius = celsius + (_first_koef * (celsius - _offset_start));
  } else if (celsius > _offset_center && celsius <= 100) {
      celsius = celsius + (_first_koef * (_offset_center - _offset_start)) + ( _second_koef * ( celsius - ( 100 - _offset_center ) )) ;
  } else if (celsius > 100) {
      celsius = celsius + _offset;
  }
  return celsius;
}
#elif PRINTER_IS_PRUSA_MINI() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX() || PRINTER_IS_PRUSA_XL_DEV_KIT() || PRINTER_IS_PRUSA_COREONEL()
constexpr float compensate_bed_temperature(float celsius) {
  return celsius;
}
#else
#error
#endif

  float scan_thermistor_table_bed(const int raw){
      return marlin_temptable_lookup(TT_NAME(THERMISTORBED), raw);
  }
  // Derived from RepRap FiveD extruder::getTemperature()
  // For bed temperature measurement.
  float Temperature::analog_to_celsius_bed(const int raw) {
    #if ENABLED(HEATER_BED_USES_THERMISTOR)
      float celsius = scan_thermistor_table_bed(raw);
      celsius = compensate_bed_temperature(celsius);
      return celsius;
    #elif HAS_MODULARBED()
      // #error dead code found by automatic analyses (see BFW-5461)
      return raw
    #else
      // #error dead code found by automatic analyses (see BFW-5461)
      return 0;
    #endif
  }
#endif // HAS_HEATED_BED

#if HAS_TEMP_BOARD
  // Derived from RepRap FiveD extruder::getTemperature()
  // For ambient temperature measurement.
  float Temperature::analog_to_celsius_board(const int raw) {
    #if ENABLED(BOARD_USES_THERMISTOR)
      return marlin_temptable_lookup(TT_NAME(TEMP_SENSOR_BOARD), raw);
    #else
      // #error dead code found by automatic analyses (see BFW-5461)
      return 0;
    #endif
  }
#endif // HAS_TEMP_BOARD

#if HAS_AC_CONTROLLER()

static void translate_ac_controller_faults(const char* pubby_name, ac_controller::Faults faults) {
  if (!faults) return;

  // Some faults are intentionally missing, because they shouldn't occur in normal printer operation.
  // We might as well BSOD when they do. No need to waste FLASH for them and error page would be pointless too.

  if (faults & ac_controller::Faults::RCD_TRIPPED) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_RCD_TRIPPED);
  if (faults & ac_controller::Faults::POWERPANIC) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_POWERPANIC);
  // ac_controller::Faults::OVERHEAT intentionally missing
  // Neither PSU NTC nor triac NTC are physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::PSU_FAN_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_PSU_FAN_NOK);
  // ac_controller::Faults::PSU_NTC_DISCONNECT intentionally missing
  // ac_controller::Faults::PSU_NTC_SHORT intentionally missing
  // PSU NTC is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::BED_NTC_DISCONNECT) return fatal_error(ErrCode::ERR_TEMPERATURE_AC_CONTROLLER_BED_NTC_DISCONNECT);
  if (faults & ac_controller::Faults::BED_NTC_SHORT) return fatal_error(ErrCode::ERR_TEMPERATURE_AC_CONTROLLER_BED_NTC_SHORT);
  // ac_controller::Faults::TRIAC_NTC_DISCONNECT intentionally missing
  // ac_controller::Faults::TRIAC_NTC_SHORT intentionally missing
  // Triac NTC is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::BED_FAN0_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_BED_FAN0_NOK);
  if (faults & ac_controller::Faults::BED_FAN1_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_BED_FAN1_NOK);
  // ac_controller::Faults::TRIAC_FAN_NOK intentionally missing
  // Triac fan is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::GRID_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_GRID_NOK);
  // ac_controller::Faults::CHAMBER_LOAD_NOK intentionally missing
  // Chamber heater is not physically present on CORE One L version of AC controller
  if (faults & ac_controller::Faults::BED_LOAD_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_BED_LOAD_NOK);
  if (faults & ac_controller::Faults::PSU_NOK) return fatal_error(ErrCode::ERR_ELECTRO_AC_CONTROLLER_PSU_NOK);
  if (faults & ac_controller::Faults::BED_RUNAWAY) return fatal_error(ErrCode::ERR_TEMPERATURE_BED_THERMAL_RUNAWAY);
  if (faults & ac_controller::Faults::MCU_OVERHEAT) return fatal_error(ErrCode::ERR_TEMPERATURE_PUBBY_MCU_OVERHEAT, pubby_name);
  if (faults & ac_controller::Faults::PCB_OVERHEAT) return fatal_error(ErrCode::ERR_TEMPERATURE_PUBBY_PCB_OVERHEAT, pubby_name);
  if (faults & ac_controller::Faults::DATA_TIMEOUT) return fatal_error(ErrCode::ERR_ELECTRO_PUBBY_DATA_TIMEOUT, pubby_name);
  if (faults & ac_controller::Faults::HEARTBEAT_MISSING) return fatal_error(ErrCode::ERR_ELECTRO_PUBBY_HEARTBEAT_MISSING, pubby_name);
  // ac_controller::Faults::UNKNOWN intentionally missing
  // This fault is only ever triggered by development version of the AC controller

  // We still want to provide some details if the fault slips to production.
  bsod("%s faults=%" PRIu32, pubby_name, static_cast<uint32_t>(faults));
}

void translate_ac_controller_faults() {
  static constexpr const char* pubby_name = "AC controller";
  if (const auto faults = buddy::puppies::ac_controller.get_faults()) {
    translate_ac_controller_faults(pubby_name, *faults);
  } else {
    fatal_error(ErrCode::ERR_SYSTEM_PUPPY_NOT_RESPONDING, pubby_name);
  }
}
#endif

/**
 * Get the raw values into the actual temperatures.
 * The raw values are created in interrupt context,
 * and this function is called from normal context
 * as it would block the stepper routine.
 */
void Temperature::updateTemperaturesFromRawValues() {
  #if HAS_HEATED_BED
    #if HAS_MODULAR_BED()
      updateModularBedTemperature();
    #elif HAS_AC_CONTROLLER()
      translate_ac_controller_faults();
      temp_bed.celsius = buddy::puppies::ac_controller.get_bed_temp().value_or(0);
    #else
      temp_bed.celsius = analog_to_celsius_bed(temp_bed.raw);
    #endif

    uint32_t now_millis = millis();
    if (temp_bed.celsius > 0.0f) {
      if (bed_frame_est_celsius < 0.0f) {
        init_bed_frame_est_celsius();
      } else {
        float dt = (now_millis - bed_frame_millis) / 1000.0f;

        // A linear function that reaches estimated bed frame temperature after
        // about 150s for 60C and about 10 minutes for 100C if starting with a
        // cold bed. With a bed already partially warmed, the time is
        // proportionally shorter.
        float step = (0.06f + (100.0f - temp_bed.celsius) * 0.0015f) * dt;
        bed_frame_est_celsius += std::clamp(temp_bed.celsius - bed_frame_est_celsius, -step, step);
      }
    }

    bed_frame_millis = now_millis;
  #endif

  #if HAS_TEMP_BOARD
    temp_board.celsius = analog_to_celsius_board(temp_board.raw);
  #endif

  #if PRINTER_IS_PRUSA_iX()
    // Both psu and ambient temperatures use the MK4 bed thermistor
    temp_psu.celsius = scan_thermistor_table_bed(temp_psu.raw);
    temp_ambient.celsius = scan_thermistor_table_bed(temp_ambient.raw);
  #endif

  // Reset the watchdog on good temperature measurement
  watchdog_refresh();

  #if HAS_TOOLCHANGER()
  if(temp_bed.celsius == 0) {
    return; // Avoid marking reading as good when the bed temperature was not read
  }
  #endif

  temperatures_ready_state = true;
  temp_meas_ready = false;
}

#define _INIT_SOFT_FAN(P) OUT_WRITE(P, FAN_INVERTING ? LOW : HIGH)
#define INIT_FAN_PIN(P) do{ if (PWM_PIN(P)) SET_PWM(P); else _INIT_SOFT_FAN(P); }while(0)

/**
 * Initialize the temperature manager
 * The manager is implemented by periodic calls to manage_heater()
 */
void Temperature::init() {

  #if EARLY_WATCHDOG
    // #error dead code found by automatic analyses (see BFW-5461)
    // Flag that the thermalManager should be running
    if (inited) return;
    inited = true;
  #endif

  #if HAS_LOCAL_BED()
    analogWrite_HEATER_BED(0);
  #endif

  #if HAS_FAN0
    INIT_FAN_PIN(FAN_PIN);
  #endif
  #if HAS_FAN1
    INIT_FAN_PIN(FAN1_PIN);
  #endif
  #if HAS_FAN2
    // #error dead code found by automatic analyses (see BFW-5461)
    INIT_FAN_PIN(FAN2_PIN);
  #endif

  #if HAS_TEMP_ADC_0
    HAL_ANALOG_SELECT(TEMP_0_PIN);
  #endif

  #if PRINTER_IS_PRUSA_iX()
    HAL_ANALOG_SELECT(TEMP_PSU_PIN);
    HAL_ANALOG_SELECT(TEMP_AMBIENT_PIN);
  #endif
  #if HAS_TEMP_HEATBREAK
    HAL_ANALOG_SELECT(TEMP_HEATBREAK_PIN);
  #endif

  HAL_timer_start(TEMP_TIMER_NUM, TEMP_TIMER_FREQUENCY);
  ENABLE_TEMPERATURE_INTERRUPT();

  for(uint8_t retry = 0; !temp_meas_ready; retry++) {
    delay(10);
    if(retry > 200) {
      bsod("Temps ded");
    }
  }

  // Wait for temperature measurement to settle
  delay(250);

  #if HAS_HEATED_BED
    minmaxtemp_raw_BED = MarlinTemptableRawMinMax::compute(TT_NAME(THERMISTORBED), BED_MINTEMP, BED_MAXTEMP);
  #endif // HAS_HEATED_BED

  manage_heater();
}

#if HAS_THERMAL_PROTECTION
  #if HAS_THERMALLY_PROTECTED_BED
    ThermalRunaway Temperature::thermal_runaway_bed;
  #endif

#endif // HAS_THERMAL_PROTECTION

void Temperature::disable_all_heaters() {
  disable_hotend();

  #if HAS_HEATED_BED
    setTargetBed(0);
    temp_bed.soft_pwm_amount = 0;
    #if HAS_LOCAL_BED()
    analogWrite_HEATER_BED(0);
    #endif
  #endif

}

void Temperature::disable_hotend() {
  for(auto tool : PhysicalToolIndex::all()) {
    Hotend::for_tool(tool).set_nozzle_target_temp(0);
  }
}

/**
 * Get raw temperatures
 */
void Temperature::set_current_temp_raw() {
  #if HAS_HEATED_BED
    temp_bed.update();
  #endif

  #if HAS_TEMP_BOARD
    temp_board.update();
  #endif

  #if PRINTER_IS_PRUSA_iX()
    temp_psu.update();
    temp_ambient.update();
  #endif

  temp_meas_ready = true;
}

void Temperature::readings_ready() {

  // Update the raw values if they've been read. Else we could be updating them during reading.
  if (!temp_meas_ready) set_current_temp_raw();

  for (auto tool : PhysicalToolIndex::all()) {
    Hotend::for_tool(tool).isr_on_readings_ready();
  }

  #if HAS_HEATED_BED
    temp_bed.reset();
  #endif

  #if HAS_TEMP_BOARD
    temp_board.reset();
  #endif

  #if PRINTER_IS_PRUSA_iX()
    temp_psu.reset();
    temp_ambient.reset();
  #endif

  #if HAS_HEATED_BED
    #if HAS_REMOTE_BED()
      //With remote bed we get temperatures in °C from controller. No raw values to check.
    #endif
    #if HAS_LOCAL_BED()
    const bool bed_on = (temp_bed.target > 0)
      #if ENABLED(PIDTEMPBED)
        || (temp_bed.soft_pwm_amount > 0)
      #endif
    ;
      minmaxtemp_raw_BED.check_temperror(temp_bed.raw, H_BED, bed_on);
    #endif
  #endif
}

/**
 * Timer 0 is shared with millies so don't change the prescaler.
 *
 * On AVR this ISR uses the compare method so it runs at the base
 * frequency (16 MHz / 64 / 256 = 976.5625 Hz), but at the TCNT0 set
 * in OCR0B above (128 or halfway between OVFs).
 *
 *  - Manage PWM to all the heaters and fan
 *  - Prepare or Measure one of the raw ADC sensor values
 *  - Check new temperature values for MIN/MAX errors (kill on error)
 *  - Step the babysteps value for each axis towards 0
 *  - For ENDSTOP_INTERRUPTS_FEATURE check endstops if flagged
 *  - Call planner.tick to count down its "ignore" time
 */
HAL_TEMP_TIMER_ISR() {
  HAL_timer_isr_prologue(TEMP_TIMER_NUM);

    Temperature::isr();

  HAL_timer_isr_epilogue(TEMP_TIMER_NUM);
}

void Temperature::isr() {
  static int8_t temp_count = -1;
  static ADCSensorState adc_sensor_state = StartupDelay;

  {
    static uint8_t pwm_count = 1;
    
    // avoid multiple loads of pwm_count
    uint8_t pwm_count_tmp = pwm_count;

    // Note: +2 to keep the "original" PWM frequency from before this change was made
    pwm_count = pwm_count_tmp + 2;

    for(auto tool : PhysicalToolIndex::all()) {
      Hotend::for_tool(tool).isr_soft_pwm(PWM255(pwm_count_tmp));
    }
  }

  ADCSensorState next_sensor_state = adc_sensor_state < SensorsReady ? (ADCSensorState)(int(adc_sensor_state) + 1) : StartSampling;

  switch (adc_sensor_state) {

    case SensorsReady: {
      // All sensors have been read. Stay in this state for a few
      // ISRs to save on calls to temp update/checking code below.
      constexpr int8_t extra_loops = MIN_ADC_ISR_LOOPS - (int8_t)SensorsReady;
      static uint8_t delay_count = 0;
      if (extra_loops > 0) {
        if (delay_count == 0) delay_count = extra_loops;  // Init this delay
        if (--delay_count)                                // While delaying...
          next_sensor_state = SensorsReady;               // retain this state (else, next state will be 0)
        break;
      }
      else {
        adc_sensor_state = StartSampling;                 // Fall-through to start sampling
        next_sensor_state = (ADCSensorState)(int(StartSampling) + 1);
      }
    }

    case StartSampling:                                   // Start of sampling loops. Do updates/checks.
      if (++temp_count >= OVERSAMPLENR) {                 // 10 * 16 * 1/(16000000/64/256)  = 164ms.
        temp_count = 0;
        readings_ready();
      }
      break;

    #if HAS_TEMP_ADC_0
      case MeasureTemp_0: temp_hotend.sample(analogRead(TEMP_0_PIN)); break;
    #endif

    #if HAS_LOCAL_BED()
      case MeasureTemp_BED: temp_bed.sample(analogRead_TEMP_BED()); break;
    #endif

    #if HAS_TEMP_HEATBREAK
      case MeasureTemp_HEATBREAK: temp_heatbreak.sample(analogRead(TEMP_HEATBREAK_PIN)); break;
    #endif

    #if HAS_TEMP_BOARD
      case MeasureTemp_BOARD: temp_board.sample(analogRead(TEMP_BOARD_PIN)); break;
    #endif

    #if PRINTER_IS_PRUSA_iX()
      case MeasureTemp_PSU: temp_psu.sample(analogRead(TEMP_PSU_PIN)); break;
      case MeasureTemp_AMBIENT: temp_ambient.sample(analogRead(TEMP_AMBIENT_PIN)); break;
    #endif

    case StartupDelay: break;

  } // switch(adc_sensor_state)

  // Go to the next state
  adc_sensor_state = next_sensor_state;

  //
  // Additional ~1KHz Tasks
  //

  #if ENABLED(BABYSTEPPING)
    babystep.task();
  #endif

  #if HAS_PLANNER()
    // Poll endstops state, if required
    endstops.poll();

    // Periodically call the planner timer
    planner.tick();
  #endif /* HAS_PLANNER() */
}

#if HAS_TEMP_SENSOR

  #include "../gcode/gcode.h"

  static void print_heater_state(const float &c, const float &t, const heater_ind_t e=INDEX_NONE) {
    char k;
    int8_t tool_nr = -1;
    #if HAS_TEMP_HOTEND
    if (e == INDEX_NONE) k = 'T';
    if (e >= H_NOZZLE_FIRST && e <= H_NOZZLE_LAST) {
      k = 'T';
      tool_nr = e - H_NOZZLE_FIRST;
    }
    #endif
    #if HAS_HEATED_BED
    if (e == H_BED) k = 'B';
    #endif
    #if HAS_TEMP_BOARD
      if (e == H_BOARD) k = 'A';
    #endif
    #if HAS_TEMP_HEATBREAK
      if (e >= H_HEATBREAK_FIRST && e <= H_HEATBREAK_LAST){
        k = 'X';
        tool_nr = e - H_HEATBREAK_FIRST;
      }
    #endif

    SERIAL_CHAR(' ');
    SERIAL_CHAR(k);
    #if HOTENDS > 1
      if (tool_nr >= 0) SERIAL_CHAR('0' + tool_nr);
    #else
      UNUSED(tool_nr);
    #endif
    SERIAL_CHAR(':');
    SERIAL_ECHO(c);
    SERIAL_ECHOPAIR("/" , t);
    delay(2);
  }

  void Temperature::print_heater_states(const uint8_t target_extruder) {
    #if HAS_TEMP_HOTEND
      print_heater_state(degHotend(target_extruder), degTargetHotend(target_extruder));
    #endif
    #if HAS_HEATED_BED
      print_heater_state(degBed(), degTargetBed(), H_BED);
    #endif

    #if HAS_TEMP_HEATBREAK
      print_heater_state(degHeatbreak(target_extruder)
          , degTargetHeatbreak(target_extruder)
        , (heater_ind_t) (H_HEATBREAK_FIRST + target_extruder)
      );
    #endif // HAS_TEMP_HEATBREAK

    #if HAS_TEMP_BOARD
      print_heater_state(degBoard()
          , 0
        , H_BOARD
      );
    #endif // HAS_TEMP_BOARD

    #if HOTENDS > 1
      for (auto tool : PhysicalToolIndex::all()) {
        print_heater_state(degHotend(tool), degTargetHotend(tool), (heater_ind_t)tool.to_raw());
      }
    #endif
    SERIAL_ECHOPAIR(" @:", getHeaterPower((heater_ind_t)target_extruder));
    #if HAS_HEATED_BED
      SERIAL_ECHOPAIR(" B@:", getHeaterPower(H_BED));
    #endif
    #if HAS_CHAMBER_API()
      auto current_chamber_temperature = buddy::chamber().current_temperature();
      auto target_chamber_temperature = buddy::chamber().target_temperature();
      if (current_chamber_temperature.has_value()) {
        SERIAL_ECHOPAIR(" C:", current_chamber_temperature.value());
        SERIAL_ECHOPAIR("/", target_chamber_temperature.has_value() ? target_chamber_temperature.value() : 0.0f);
      }
    #endif

    #if HAS_TEMP_HEATBREAK
      SERIAL_ECHOPAIR(" HBR@:", getHeaterPower((heater_ind_t)(H_HEATBREAK_FIRST + target_extruder)));
    #endif
    #if HOTENDS > 1
      for (auto tool : PhysicalToolIndex::all()) {
        SERIAL_ECHOPAIR(" @", tool.to_raw());
        SERIAL_CHAR(':');
        SERIAL_ECHO(getHeaterPower((heater_ind_t)tool.to_raw()));
      }
    #endif

    // Detailed modular bed report
    #if HAS_MODULAR_BED()
      for(int y = 0; y < Y_HBL_COUNT; ++y) {
        for(int x = 0; x < X_HBL_COUNT; ++x) {
          SERIAL_ECHO(" B_");
          SERIAL_ECHO(x);
          SERIAL_CHAR('_');
          SERIAL_ECHO(y);
          SERIAL_CHAR(':');
          SERIAL_ECHO(advanced_modular_bed->get_temp(x, y));
          SERIAL_CHAR('/');
          SERIAL_ECHO(advanced_modular_bed->get_target(x, y));
          SERIAL_FLUSH();
        }
      }
    #endif
  }

  #if ENABLED(AUTO_REPORT_TEMPERATURES)

    uint8_t Temperature::auto_report_temp_interval;
    millis_t Temperature::next_temp_report_ms;

    void Temperature::auto_report_temperatures() {
      if (auto_report_temp_interval && ELAPSED(millis(), next_temp_report_ms)) {
        // Do not log heater states, only print to serial
        SerialLoggingDisabler sld;

        next_temp_report_ms = millis() + 1000UL * auto_report_temp_interval;
        PORT_REDIRECT(SERIAL_BOTH);
        print_heater_states(active_extruder);
        SERIAL_EOL();
      }
    }

  #endif // AUTO_REPORT_TEMPERATURES

  #if HAS_TEMP_HOTEND

    #ifndef MIN_COOLING_SLOPE_DEG
      #define MIN_COOLING_SLOPE_DEG 1.50
    #endif
    #ifndef MIN_COOLING_SLOPE_TIME
      #define MIN_COOLING_SLOPE_TIME 60
    #endif

    bool Temperature::are_hotend_temperatures_reached() {
      for (auto tool : PhysicalToolIndex::all()) {
        if (!Hotend::for_tool(tool).is_nozzle_temp_reached()) {
            return false;
        }
      }

      return true;
    }

    void Temperature::setTargetHotend(const int16_t celsius, const uint8_t tool) {
      Hotend::for_tool(tool).set_nozzle_target_temp(celsius);
    }

    bool Temperature::wait_for_hotend(const uint8_t target_extruder, WaitForHotendParams params) {      
      const auto target_tool = PhysicalToolIndex::from_raw_notool(target_extruder);
#if HAS_INDX()
      // The INDX is unable to read temperature of a tool that isn't picked.
      // Therefore it can't wait for its temperature.
      const auto current_tool = PhysicalToolIndex::currently_selected();
      if (std::holds_alternative<NoTool>(target_tool) || std::holds_alternative<NoTool>(current_tool) || target_tool != current_tool) {
        return false;
      }
#endif

      Hotend &hotend = Hotend::for_tool(target_tool);

      // When skip_residency is set (M109 `C`), the target counts as reached the
      // moment the nozzle enters the temperature window, without waiting for the
      // residency settle. Evaluated here so the Hotend keeps no extra state.
      const auto target_reached = [&]() {
        if (!params.skip_residency) {
          return hotend.is_nozzle_temp_reached();
        }
        const auto current = hotend.nozzle_temp();
        return hotend.nozzle_target_temp() <= 0
            || std::abs(hotend.nozzle_target_temp() - current) < TEMP_WINDOW;
      };

      #if BOARD_IS_MASTER_BOARD()
        // Keep all heaters on while we're waiting for temperatures
        buddy::SafetyTimerBlocker safety_timer_blocker;
      #endif

      // Loop until the temperature has stabilized

      #if DISABLED(BUSY_WHILE_HEATING) && ENABLED(HOST_KEEPALIVE_FEATURE)
        // #error dead code found by automatic analyses (see BFW-5461)
        KEEPALIVE_STATE(NOT_BUSY);
      #endif

      float target_temp = -1.0, old_temp = 9999.0;
      bool wants_to_cool = false;
      wait_for_heatup = true;
      millis_t now, next_temp_ms = 0, next_cool_check_ms = 0;

      /// !!! PRINT FAN IS ALWAYS FAN 0
      const uint8_t fan_speed_at_start = get_fan_speed(0);
      ScopeGuard fan_restore_guard = [&] {
        thermalManager.set_fan_speed(0, fan_speed_at_start);
      };

      PrintStatusMessageGuard statusGuard;

      do {
        #if HAS_PLANNER()
          // Check if we're aborting
          if (planner.draining()) break;
        #endif

        // Target temperature might be changed during the loop
        if (target_temp != degTargetHotend(target_extruder)) {
          wants_to_cool = hotend.nozzle_target_temp() < hotend.nozzle_temp();
          target_temp = degTargetHotend(target_extruder);

          // Exit if S<lower>, continue if S<higher>, R<lower>, or R<higher>
          if (params.no_wait_for_cooling && wants_to_cool) break;

          // If fan_cooling is enabled, assist the cooling/heating with the print fan
          // !!! ONLY WORKS FOR ACTIVE EXTRUDER - PRINT FAN IS ALWAYS FAN 0
          if (params.fan_cooling && active_extruder == target_extruder)
            thermalManager.set_fan_speed(0, wants_to_cool ? 255 : 0);
        }

        now = millis();
        if (ELAPSED(now, next_temp_ms)) { // Print temp & remaining time every 1s while waiting
          // Do not log heater states, only print to serial
          SerialLoggingDisabler sld;

          next_temp_ms = now + 1000UL;
          print_heater_states(target_extruder);
          SERIAL_ECHOPGM(" W:?");
          SERIAL_EOL();
        }

        idle(true);

        const float temp = degHotend(target_extruder);
        statusGuard.update<PrintStatusMessage::waiting_for_hotend_temp>({.progress{ .current = temp, .target = target_temp }, .tool=target_extruder});

        // Prevent a wait-forever situation if R is misused i.e. M109 R0
        if (wants_to_cool) {
          // break after MIN_COOLING_SLOPE_TIME seconds
          // if the temperature did not drop at least MIN_COOLING_SLOPE_DEG
          if (!next_cool_check_ms || ELAPSED(now, next_cool_check_ms)) {
            if (old_temp - temp < float(MIN_COOLING_SLOPE_DEG)) break;
            next_cool_check_ms = now + 1000UL * MIN_COOLING_SLOPE_TIME;
            old_temp = temp;
          }
        }
      } while (wait_for_heatup && !target_reached());

      return wait_for_heatup;
    }

  #endif // HAS_TEMP_HOTEND

  #if HAS_HEATED_BED

    #ifndef MIN_COOLING_SLOPE_DEG_BED
      #define MIN_COOLING_SLOPE_DEG_BED 1.50
    #endif
    #ifndef MIN_COOLING_SLOPE_TIME_BED
      #define MIN_COOLING_SLOPE_TIME_BED 60
    #endif

    bool Temperature::is_bed_temperature_reached() {
      // TODO: Switch to residency time and employ with wait_for_bed
      // To achieve that, we will have to take the residency out of wait_for_bed and make it global
      return temp_bed.target <= 0 || std::abs(temp_bed.target - temp_bed.celsius) <= TEMP_BED_HYSTERESIS;
    }

    void Temperature::setTargetBed(const int16_t celsius) {
      // We cannot overwrite target temps while the safety_timer is active, deactivate it first
      buddy::safety_timer().reset_restore_nonblocking();

    #if ENABLED(AUTO_POWER_CONTROL)
        if (celsius) {
            powerManager.power_on();
        }
    #endif
        temp_bed.target =
    #ifdef BED_MAXTEMP
            _MIN(celsius, BED_MAXTEMP - BED_MAXTEMP_SAFETY_MARGIN)
    #else
      // #error dead code found by automatic analyses (see BFW-5461)
            celsius
    #endif
            ;

    #if HAS_MODULAR_BED()
        for (uint8_t x = 0; x < X_HBL_COUNT; ++x) {
            for (uint8_t y = 0; y < Y_HBL_COUNT; ++y) {
                int16_t target_temp = 0;
                if (temp_bed.enabled_mask & (1 << advanced_modular_bed->idx(x, y))) {
                    target_temp = temp_bed.target;
                }
                advanced_modular_bed->set_target(x, y, target_temp);
            }
        }
        advanced_modular_bed->update_bedlet_temps(temp_bed.enabled_mask, temp_bed.target);
    #endif

    #if HAS_AC_CONTROLLER()
        buddy::puppies::ac_controller.set_bed_target_temp(temp_bed.target);
    #endif

    #if WATCH_BED
        watch_bed.arm(temp_bed.target);
    #endif
    }


    bool Temperature::wait_for_bed(const bool no_wait_for_cooling/*=true*/) {
      // TODO: Employ is_bed_temperature_reached once it considers residency
      
      // Keep all heaters on while we're waiting for temperatures
      buddy::SafetyTimerBlocker safety_timer_blocker;

      #if TEMP_BED_RESIDENCY_TIME > 0
        millis_t residency_start_ms = 0;
        bool first_loop = true;
        // Loop until the temperature has stabilized
        #define TEMP_BED_CONDITIONS (!residency_start_ms || PENDING(now, residency_start_ms + (TEMP_BED_RESIDENCY_TIME) * 1000UL))
      #else
        // #error dead code found by automatic analyses (see BFW-5461)
        // Loop until the temperature is very close target
        #define TEMP_BED_CONDITIONS (wants_to_cool ? isCoolingBed() : isHeatingBed())
      #endif

      float target_temp = -1, old_temp = 9999;
      bool wants_to_cool = false;
      wait_for_heatup = true;
      millis_t now, next_temp_ms = 0, next_cool_check_ms = 0;

      #if DISABLED(BUSY_WHILE_HEATING) && ENABLED(HOST_KEEPALIVE_FEATURE)
        // #error dead code found by automatic analyses (see BFW-5461)
        KEEPALIVE_STATE(NOT_BUSY);
      #endif

      PrintStatusMessageGuard statusGuard;

      do {
        // Check if we're aborting
        if (planner.draining()) break;

        // Target temperature might be changed during the loop
        if (target_temp != degTargetBed()) {
          wants_to_cool = isCoolingBed();
          target_temp = degTargetBed();

          // Exit if S<lower>, continue if S<higher>, R<lower>, or R<higher>
          if (no_wait_for_cooling && wants_to_cool) break;
        }

        now = millis();
        if (ELAPSED(now, next_temp_ms)) { //Print Temp Reading every 1 second while heating up.
          // Do not log heater states, only print to serial
          SerialLoggingDisabler sld;

          next_temp_ms = now + 1000UL;
          print_heater_states(active_extruder);
          #if TEMP_BED_RESIDENCY_TIME > 0
            SERIAL_ECHOPGM(" W:");
            if (residency_start_ms)
              SERIAL_ECHO(long((((TEMP_BED_RESIDENCY_TIME) * 1000UL) - (now - residency_start_ms)) / 1000UL));
            else
              SERIAL_CHAR('?');
          #endif
          SERIAL_EOL();
        }

        idle(true);

        const float temp = degBed();
        statusGuard.update<PrintStatusMessage::waiting_for_bed_temp>({.current = temp, .target = target_temp});

        #if TEMP_BED_RESIDENCY_TIME > 0

          const float temp_diff = ABS(target_temp - temp);

          if (!residency_start_ms) {
            // Start the TEMP_BED_RESIDENCY_TIME timer when we reach target temp for the first time.
            if (temp_diff < TEMP_BED_WINDOW) {
              residency_start_ms = now;
              if (first_loop) residency_start_ms += (TEMP_BED_RESIDENCY_TIME) * 1000UL;
            }
          }
          else if (temp_diff > TEMP_BED_HYSTERESIS) {
            // Restart the timer whenever the temperature falls outside the hysteresis.
            residency_start_ms = now;
          }

        #endif // TEMP_BED_RESIDENCY_TIME > 0

        // Prevent a wait-forever situation if R is misused i.e. M190 R0
        if (wants_to_cool) {
          // Break after MIN_COOLING_SLOPE_TIME_BED seconds
          // if the temperature did not drop at least MIN_COOLING_SLOPE_DEG_BED
          if (!next_cool_check_ms || ELAPSED(now, next_cool_check_ms)) {
            if (old_temp - temp < float(MIN_COOLING_SLOPE_DEG_BED)) break;
            next_cool_check_ms = now + 1000UL * MIN_COOLING_SLOPE_TIME_BED;
            old_temp = temp;
          }
        }

        #if TEMP_BED_RESIDENCY_TIME > 0
          first_loop = false;
        #endif

      } while (wait_for_heatup && TEMP_BED_CONDITIONS);

      return wait_for_heatup;
    }

    void Temperature::init_bed_frame_est_celsius() {
        if (temp_bed.celsius < room_temperature) {
          // If around room temperature, init directly to bed temperature
          bed_frame_est_celsius = temp_bed.celsius;
        } else {
          // If over room temp, init with a fraction of the current temp that's
          // over room temperature, as a crude estimation of how the bed frame
          // has been heated up
          bed_frame_est_celsius = room_temperature + (temp_bed.celsius - room_temperature) * 0.7f;
        }
    }

    void Temperature::wait_for_frame_heatup() {
        // Keep everything heated up when absorbing heat
        buddy::SafetyTimerBlocker safety_timer_blocker;

        constexpr float finish_threshold = 1.0f;

        if (fabs(temp_bed.target - bed_frame_est_celsius) < finish_threshold) {
            log_info(MarlinServer, "Absorbing heat: already stable, continuing");
            return;
        }

        if (marlin_debug_flags & MARLIN_DEBUG_DRYRUN) {
            // In dry run, the bed is left cold. The temperature would never stabilize.
            return;
        }

        if (temp_bed.target < bed_frame_est_celsius) {
          // Do not wait for cooldown. Cooling is slow and propagated evenly across the bed, it won't warp the bed differently.
          log_info(MarlinServer, "Absorbing heat: target lower than actual temp, continuing");
          return;
        }

        if (temp_bed.target <= room_temperature) {
          log_info(MarlinServer, "Absorbing heat: target lower than room temp, continuing");
          return;
        }

        SkippableGCode::Guard skippable_operation;
        PrintStatusMessageGuard status_guard;

        float start_target = temp_bed.target;
        // potential negative start_diff is handled by the clamp on progress
        float start_diff = fabs(start_target - bed_frame_est_celsius) - finish_threshold;
        float progress = 0.f;
        while (fabs(temp_bed.target - bed_frame_est_celsius) > finish_threshold && !skippable_operation.is_skip_requested()) {
            // Check if we're aborting
            if (planner.draining()) {
                break;
            }
            if (start_target != temp_bed.target) {
              //Target changed -> recalculate start_diff
              start_target = temp_bed.target;
              start_diff = fabs(start_target - bed_frame_est_celsius) - finish_threshold;
            }

            idle(true);

            progress = std::max(progress, std::clamp(100 - ((fabs(temp_bed.target - bed_frame_est_celsius) - finish_threshold) / start_diff) * 100, 0.f, 100.f));

            status_guard.update<PrintStatusMessage::absorbing_heat>({ .current = progress, .target = 100 });
        }

        MarlinUI::reset_status();
    }

  #endif // HAS_HEATED_BED

#endif // HAS_TEMP_SENSOR

#if HAS_MODULAR_BED()
void Temperature::updateModularBedTemperature(){
      float sum = 0;
      uint8_t count = 0;
      for(uint8_t x = 0; x < X_HBL_COUNT; ++x) {
        for(uint8_t y = 0; y < Y_HBL_COUNT; ++y) {
          if(temp_bed.enabled_mask & (1 << advanced_modular_bed->idx(x, y))) {
            sum += advanced_modular_bed->get_temp(x, y);
            count++;
          }
        }
      }
      temp_bed.celsius = sum / count;
}
#endif

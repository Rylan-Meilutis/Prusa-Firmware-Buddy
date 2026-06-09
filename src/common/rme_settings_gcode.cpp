#include "rme_settings_gcode.hpp"

#include <config_store/store_instance.hpp>
#include <common/extended_printer_type.hpp>
#include <leds/light_state.hpp>
#include <option/has_i2c_expander.h>
#include <option/has_leds.h>
#include <option/has_side_leds.h>
#include <printers.h>
#include <serial_printing_ui_mode.hpp>

#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    #include <leds/external_light_bar.hpp>
#endif

namespace {

uint8_t packed_percent(uint32_t values, leds::LightState state) {
    uint8_t value = (values >> leds::light_state_shift(state)) & 0xff;
    return value > 100 ? 100 : value;
}

uint8_t packed_screen_percent(uint32_t values, leds::LightState state) {
    return leds::clamp_screen_brightness(state, packed_percent(values, state));
}

#if HAS_SIDE_LEDS()
uint8_t pwm_to_percent(uint8_t value) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * 100 + 127) / 255);
}
#endif

} // namespace

namespace rme_settings_gcode {

bool write(FILE *file) {
    if (!file) {
        return false;
    }

    bool ok = true;
    ok &= fprintf(file, "M86 S%u B%u\n",
        config_store().hotend_heater_safety_timeout_s.get(),
        config_store().bed_heater_safety_timeout_s.get()) >= 0;

    const uint32_t screen = config_store().screen_brightness_by_state.get();
    ok &= fprintf(file, "M154.0 D%u I%u A%u P%u\n",
        packed_screen_percent(screen, leds::LightState::deep_idle),
        packed_screen_percent(screen, leds::LightState::idle),
        packed_screen_percent(screen, leds::LightState::active),
        packed_screen_percent(screen, leds::LightState::printing)) >= 0;

#if HAS_SIDE_LEDS()
    ok &= fprintf(file, "M154.1 D%u I%u A%u P%u\n",
        pwm_to_percent(config_store().side_leds_deep_idle_brightness.get()),
        pwm_to_percent(config_store().side_leds_dimmed_brightness.get()),
        pwm_to_percent(config_store().side_leds_max_brightness.get()),
        pwm_to_percent(config_store().side_leds_print_brightness.get())) >= 0;
    ok &= fprintf(file, "M154.3 A%u E%u O%u H%u F%u\n",
        config_store().side_leds_activity_timeout_s.get(),
        config_store().side_leds_event_timeout_s.get(),
        config_store().side_leds_off_timeout_s.get(),
        config_store().side_leds_door_holds_active.get() ? 1 : 0,
        config_store().post_print_led_hold_enabled.get() ? 1 : 0) >= 0;
#endif

#if HAS_LEDS()
    const uint32_t status = config_store().status_led_brightness_by_state.get();
    ok &= fprintf(file, "M154.2 D%u I%u A%u P%u\n",
        packed_percent(status, leds::LightState::deep_idle),
        packed_percent(status, leds::LightState::idle),
        packed_percent(status, leds::LightState::active),
        packed_percent(status, leds::LightState::printing)) >= 0;
    ok &= fprintf(file, "M154.7 H%u\n", config_store().status_led_finished_hold_s.get()) >= 0;
#endif

    ok &= fprintf(file, "M154.4 U%u T%u A%u S%u\n",
        static_cast<unsigned>(config_store().serial_print_ui_mode.get()),
        config_store().serial_print_timeout_s.get(),
        config_store().serial_print_auto_start.get() ? 1 : 0,
        config_store().serial_print_screen_enabled.get() ? 1 : 0) >= 0;

#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    ok &= fprintf(file, "M154.5 D%u I%u A%u P%u\n",
        leds::external_light_bar::state_enabled(leds::LightState::deep_idle) ? 1 : 0,
        leds::external_light_bar::state_enabled(leds::LightState::idle) ? 1 : 0,
        leds::external_light_bar::state_enabled(leds::LightState::active) ? 1 : 0,
        leds::external_light_bar::state_enabled(leds::LightState::printing) ? 1 : 0) >= 0;
#endif

#if HAS_EXTENDED_PRINTER_TYPE()
    ok &= fprintf(file, "M154.6 E%u\n", config_store().extended_printer_type.get()) >= 0;
#endif

    return ok;
}

} // namespace rme_settings_gcode

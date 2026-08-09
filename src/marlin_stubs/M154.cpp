/**
 * @file
 */

#include "PrusaGcodeSuite.hpp"

#include "../../lib/Marlin/Marlin/src/gcode/parser.h"

#include <config_store/store_instance.hpp>
#include <common/extended_printer_type.hpp>
#include <leds/light_state.hpp>
#include <option/has_chamber_filtration_api.h>
#include <option/has_i2c_expander.h>
#include <option/has_leds.h>
#include <option/has_side_leds.h>
#include <serial_printing_ui_mode.hpp>
#include <cstddef>

#if HAS_LEDS()
    #include <leds/status_leds_handler.hpp>
#endif
#if HAS_SIDE_LEDS()
    #include <leds/side_strip_handler.hpp>
#endif
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    #include <leds/external_light_bar.hpp>
#endif
#if HAS_CHAMBER_FILTRATION_API()
    #include <feature/chamber_filtration/chamber_filtration.hpp>
#endif
namespace {

constexpr char state_codes[] = { 'D', 'I', 'A', 'P' };
constexpr leds::LightState states[] = {
    leds::LightState::deep_idle,
    leds::LightState::idle,
    leds::LightState::active,
    leds::LightState::printing,
};
constexpr size_t state_count = sizeof(state_codes) / sizeof(state_codes[0]);

uint8_t percent_value(char code) {
    const uint8_t value = parser.byteval(code);
    return value > 100 ? 100 : value;
}

#if HAS_SIDE_LEDS()
uint8_t brightness_percent_to_pwm(uint8_t value) {
    return value == 100 ? 255 : static_cast<uint8_t>(static_cast<uint16_t>(value) * 255 / 100);
}
#endif

#if !HAS_SIDE_LEDS()
void set_packed_percent(uint32_t &values, leds::LightState state, uint8_t value) {
    const uint8_t shift = leds::light_state_shift(state);
    values = (values & ~(0xffu << shift)) | (static_cast<uint32_t>(value) << shift);
}
#endif

void set_screen_brightness_from_parser(char code, leds::LightState state) {
    if (!parser.seenval(code)) {
        return;
    }

    const uint8_t value = leds::clamp_screen_brightness(state, percent_value(code));
#if HAS_SIDE_LEDS()
    leds::SideStripHandler::instance().set_screen_brightness(state, value);
#else
    uint32_t values = config_store().screen_brightness_by_state.get();
    set_packed_percent(values, state, value);
    config_store().screen_brightness_by_state.set(values);
#endif
}

void set_screen_brightness_group() {
    for (size_t i = 0; i < state_count; ++i) {
        set_screen_brightness_from_parser(state_codes[i], states[i]);
    }
#if HAS_SIDE_LEDS()
    leds::SideStripHandler::instance().activity_ping();
#endif
}

#if HAS_SIDE_LEDS()
void set_side_brightness_from_parser(char code, leds::LightState state) {
    if (parser.seenval(code)) {
        leds::SideStripHandler::instance().set_brightness(state, brightness_percent_to_pwm(percent_value(code)));
    }
}

void set_side_brightness_group() {
    for (size_t i = 0; i < state_count; ++i) {
        set_side_brightness_from_parser(state_codes[i], states[i]);
    }
    leds::SideStripHandler::instance().activity_ping();
}

void set_side_timeout_group() {
    if (parser.seenval('A')) {
        leds::SideStripHandler::instance().set_activity_timeout_s(parser.ushortval('A'));
    }
    if (parser.seenval('E')) {
        leds::SideStripHandler::instance().set_event_timeout_s(parser.ushortval('E'));
    }
    if (parser.seenval('O')) {
        leds::SideStripHandler::instance().set_off_timeout_s(parser.ushortval('O'));
    }
    if (parser.seenval('H')) {
        leds::SideStripHandler::instance().set_door_holds_active(parser.boolval('H'));
    }
    if (parser.seenval('F')) {
        leds::SideStripHandler::instance().set_post_print_hold_enabled(parser.boolval('F'));
    }
}
#endif

#if HAS_LEDS()
void set_status_brightness_from_parser(char code, leds::LightState state) {
    if (parser.seenval(code)) {
        leds::StatusLedsHandler::instance().set_brightness(state, percent_value(code));
    }
}

void set_status_brightness_group() {
    for (size_t i = 0; i < state_count; ++i) {
        set_status_brightness_from_parser(state_codes[i], states[i]);
    }
}

void set_status_timeout_group() {
    if (parser.seenval('H')) {
        leds::StatusLedsHandler::instance().set_finished_hold_s(parser.ushortval('H'));
    }
}
#endif

#if HAS_CHAMBER_FILTRATION_API()
void set_filtration_cycle_group() {
    if (parser.seenval('S') && !parser.boolval('S')) {
        buddy::chamber_filtration().stop_post_print_filtration();
    } else {
        buddy::chamber_filtration().start_post_print_filtration();
    }
}
#endif

void set_serial_group() {
    if (parser.seenval('U')) {
        const uint8_t mode = parser.byteval('U');
        if (mode <= static_cast<uint8_t>(SerialPrintingUiMode::_last)) {
            const auto ui_mode = static_cast<SerialPrintingUiMode>(mode);
            config_store().serial_print_ui_mode.set(ui_mode);
            config_store().serial_print_legacy_ui.set(ui_mode == SerialPrintingUiMode::legacy);
        }
    }
    if (parser.seenval('T')) {
        config_store().serial_print_timeout_s.set(parser.ushortval('T'));
    }
    if (parser.seenval('A')) {
        config_store().serial_print_auto_start.set(parser.boolval('A'));
    }
    if (parser.seenval('S')) {
        config_store().serial_print_screen_enabled.set(parser.boolval('S'));
    }
}

#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
void set_external_state_group() {
    for (size_t i = 0; i < state_count; ++i) {
        if (parser.seenval(state_codes[i])) {
            leds::external_light_bar::set_state_enabled(states[i], parser.boolval(state_codes[i]));
        }
    }
}
#endif

#if HAS_EXTENDED_PRINTER_TYPE()
void set_extended_printer_type_group() {
    if (parser.seenval('E')) {
        const uint8_t index = parser.byteval('E');
        if (index < extended_printer_type_model.size()) {
            config_store().extended_printer_type.set(index);
        }
    }
}
#endif

} // namespace

/**
 *### M154: RME firmware settings
 *
 * Subcodes:
 * - `M154.0 D I A P` screen brightness percent: deep idle, idle, active, printing
 * - `M154.1 D I A P` chamber/side brightness percent
 * - `M154.2 D I A P` status LED brightness percent
 * - `M154.3 A E O H F` lighting timeouts/flags
 * - `M154.4 U T A S` serial UI mode, timeout, auto-start, screen enable
 * - `M154.5 D I A P` external light bar state enables
 * - `M154.6 E` extended printer type index
 * - `M154.7 H` status LED finished hold seconds
 * - `M154.8 [S]` trigger/stop configured post-print filtration; `S0` stops
 */
void PrusaGcodeSuite::M154() {
    switch (parser.subcode) {
    case 0:
        set_screen_brightness_group();
        break;
#if HAS_SIDE_LEDS()
    case 1:
        set_side_brightness_group();
        break;
#endif
#if HAS_LEDS()
    case 2:
        set_status_brightness_group();
        break;
#endif
#if HAS_SIDE_LEDS()
    case 3:
        set_side_timeout_group();
        break;
#endif
    case 4:
        set_serial_group();
        break;
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    case 5:
        set_external_state_group();
        break;
#endif
#if HAS_EXTENDED_PRINTER_TYPE()
    case 6:
        set_extended_printer_type_group();
        break;
#endif
#if HAS_LEDS()
    case 7:
        set_status_timeout_group();
        break;
#endif
#if HAS_CHAMBER_FILTRATION_API()
    case 8:
        set_filtration_cycle_group();
        break;
#endif
    default:
        break;
    }
}

/**
 * @file
 */
#include "PrusaGcodeSuite.hpp"
#include "../../lib/Marlin/Marlin/src/gcode/parser.h"
#include "leds/status_leds_handler.hpp"
#if HAS_SIDE_LEDS()
    #include "leds/side_strip_handler.hpp"
#endif
#include <option/has_i2c_expander.h>
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
    #include <leds/external_light_bar.hpp>
    #include "hwio_pindef.h"
#endif
#include <optional>

std::optional<leds::ColorRGBW> parse_color() {
    if (parser.seen('R') && parser.seen('G') && parser.seen('B')) {
        uint8_t R = parser.byteval('R');
        uint8_t G = parser.byteval('G');
        uint8_t B = parser.byteval('B');
        uint8_t W = parser.byteval('W', 0);
        return leds::ColorRGBW { R, G, B, W };

    } else if (parser.seen('S') && parser.seen('H') && parser.seen('V')) {
        float H = parser.floatval('H');
        float S = parser.floatval('S');
        float V = parser.floatval('V');
        auto color = leds::ColorRGBW::from_hsv({ H, S, V });
        color.w = parser.byteval('W', 0);
        return color;
    } else if (parser.seen('W')) {
        uint8_t W = parser.byteval('W');
        return leds::ColorRGBW { 0, 0, 0, W };
    }
    return std::nullopt;
}

/**
 * @brief Set display led animations
 *
 * Color input supports RGB and HSV format
 *
 */

/**
 * \addtogroup G-Codes
 */
/**
 *### M150: Set LED color <a href="https://reprap.org/wiki/G-code#M150:_Set_LED_color">M150: Set LED color</a>
 *
 *#### Usage
 *
 *    M150 [ R | B | G | H | S | V | A | S | P ]
 *
 *#### Parameters
 *
 * RGB color space
 * - `R` - Red intensity from 0 to 255
 * - `G` - Green intensity from 0 to 255
 * - `B` - Blue intensity from 0 to 255
 *
 * HSV color space
 * - `H` - Hue from 0 to 360
 * - `S` - Saturation from 0 to 100
 * - `V` - Saturation form 0 to 100
 *
 * Effect
 * - `A` - Animation type
 *   - `0` - Solid color
 *   - `1` - Pulsing
 * - `S` - Printer state
 *   - `0` - Idle
 *   - `1` - Printing
 *   - `2` - Pausing
 *   - `3` - Resuming
 *   - `4` - Aborting
 *   - `5` - Finishing
 *   - `6` - Warning
 *   - `7` - PowerPanic
 *
 * - `P` - Period in ms
 */
void PrusaGcodeSuite::M150() {
    if (parser.seen('S')) {
        uint16_t state = parser.byteval('S');
        if (state > static_cast<uint8_t>(leds::StateAnimation::_last)) {
            return;
        }
        leds::StatusLedsHandler::instance().set_animation(static_cast<leds::StateAnimation>(state));
    } else if (parser.seen('A')) {
        uint8_t animation = parser.byteval('A');
        if (animation > static_cast<uint8_t>(leds::AnimationType::_last)) {
            return;
        }
        auto color = parse_color();
        if (color) {
            uint16_t period = parser.ushortval('P', 0);
            leds::StatusLedsHandler::instance().set_custom_animation(*color, static_cast<leds::AnimationType>(animation), period);
        }
    }
}

/**
 *### M151: Set LED strip <a href="https://reprap.org/wiki/G-code#M151:_Set_LED_strip">M151: Set LED strip</a>
 *
 * Only XL and iX
 *
 *#### Usage
 *
 *    M151 [ R | B | G | W | H | S | V | D | T ]
 *
 *#### Parameters
 *
 * RGB color space
 * - `R` - Red intensity from 0 to 255
 * - `G` - Green intensity from 0 to 255
 * - `B` - Blue intensity from 0 to 255
 * - `W` - White intensity from 0 to 255
 *
 * HSV color space
 * - `H` - Hue from 0 to 360
 * - `S` - Saturation from 0 to 100
 * - `V` - Saturation form 0 to 100
 *
 * Effect
 * - `A` - active white brightness. User interaction and door activity use this brightness.
 * - `P` - print/minimum white brightness. User activity can raise the effective brightness above this.
 * - `L` - idle/dimmed white brightness.
 * - `D` - duration in milliseconds, set to 0 for infinite duration
 * - `T` - transition in milliseconds (fade in / fade out)
 * - `I` - time until dimmed in seconds after user / door / print activity, 0 to dim immediately
 * - `E` - compatibility alias for `I`
 * - `O` - off timeout in seconds after last user / door / print activity, 0 to stay dimmed
 *
 * Fade in is counted toward duration,
 * so if duration is greater than 0 and less than transition,
 * effect doesn't reach full color intensity.
 * Fade out is not counted toward duration.
 */
#if HAS_SIDE_LEDS()
void PrusaGcodeSuite::M151() {
    const bool has_rgb = parser.seen('R') && parser.seen('G') && parser.seen('B');
    const bool has_hsv = parser.seen('H') && parser.seen('S') && parser.seen('V');
    if (parser.seenval('A') && !has_rgb && !has_hsv) {
        leds::SideStripHandler::instance().set_max_brightness(parser.value_byte());
    }
    if (parser.seenval('P') && !has_rgb && !has_hsv) {
        leds::SideStripHandler::instance().set_print_brightness(parser.value_byte());
    }
    if (parser.seenval('L') && !has_rgb && !has_hsv) {
        leds::SideStripHandler::instance().set_dimmed_brightness(parser.value_byte());
    }
    if (parser.seenval('I')) {
        leds::SideStripHandler::instance().set_activity_timeout_s(parser.value_ushort());
    }
    if (parser.seenval('E')) {
        leds::SideStripHandler::instance().set_activity_timeout_s(parser.value_ushort());
    }
    if (parser.seenval('O')) {
        leds::SideStripHandler::instance().set_off_timeout_s(parser.value_ushort());
    }

    if (has_rgb || has_hsv) {
        auto color_val = parse_color().value();
        uint32_t duration = parser.ulongval('D', 400);
        uint32_t transition = parser.ulongval('T', 100);
        leds::SideStripHandler::instance().set_custom_color(color_val, duration, transition);
    }
}

/** @}*/

#endif

/**
 *### M153: Mark serial host idle
 *
 * Clears side LED activity for hosts that know they are no longer actively streaming
 * useful work, without ending the serial print state.
 */
#if HAS_SIDE_LEDS()
void PrusaGcodeSuite::M153() {
    leds::SideStripHandler::instance().idle_ping();
}
#endif

/**
 *### M152: Set external GPIO light bar
 *
 * Only xBuddy boards with the GPIO breakout IO expander.
 *
 *#### Usage
 *
 *    M152 [ P | S ]
 *
 *#### Parameters
 *
 * - `P` - IO expander pin <0;7>. Required.
 * - `S` - Light bar control, 0 = not used for light bar, 1 = used for light bar.
 * - `A` - Output mode, 1 = active high, 0 = active sink / active low. Pins 0-3 only allow active sink.
 * - `F` - Hold external light output for diagnostics, 1 = on, 0 = off. Use `F2` to clear.
 */
#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()
void PrusaGcodeSuite::M152() {
    if (!parser.seenval('P')) {
        SERIAL_ECHOLNPGM("M152 Bad request: P is required");
        return;
    }

    const auto pin = parser.byteval('P');
    if (pin >= buddy::hw::TCA6408A::pin_count || !leds::external_light_bar::pin_supports_output(pin)) {
        SERIAL_ECHOLNPGM("M152 Bad request: P out of range <0,7>");
        return;
    }

    auto mode = leds::external_light_bar::pin_mode(pin);
    const bool configure = parser.seenval('A') || parser.seenval('S');
    if (parser.seenval('A')) {
        mode = parser.value_bool() ? leds::external_light_bar::OutputMode::active_high : leds::external_light_bar::OutputMode::pull_down;
    }
    if (parser.seenval('S')) {
        mode = parser.value_bool() ? (mode == leds::external_light_bar::OutputMode::off ? leds::external_light_bar::OutputMode::pull_down : mode) : leds::external_light_bar::OutputMode::off;
    }

    if (configure && !leds::external_light_bar::set_pin_mode(pin, mode)) {
        SERIAL_ECHOLNPGM("M152 Bad request: selected pin does not support requested output mode");
        return;
    }

    if (parser.seenval('F')) {
        const auto force = parser.value_byte();
        if (force <= 1) {
            leds::external_light_bar::set_diagnostic_override(true, force != 0);
        } else {
            leds::external_light_bar::set_diagnostic_override(false, false);
        }
    }

    uint8_t config_register = 0;
    uint8_t output_register = 0;
    uint8_t input_register = 0;
    buddy::hw::io_expander2.read_reg(buddy::hw::TCA6408A::Register::Config, config_register);
    buddy::hw::io_expander2.read_reg(buddy::hw::TCA6408A::Register::Output, output_register);
    buddy::hw::io_expander2.read_reg(buddy::hw::TCA6408A::Register::Input, input_register);

    SERIAL_ECHO_START();
    SERIAL_ECHOLNPAIR(" External light bar pin: ", pin, " mode: ", mode == leds::external_light_bar::OutputMode::off ? "off" : (mode == leds::external_light_bar::OutputMode::active_high ? "active high" : "active sink"));
    SERIAL_ECHOLNPAIR(" External light bar mask: ", leds::external_light_bar::protected_pin_mask(), " active high mask: ", leds::external_light_bar::active_high_pin_mask());
    SERIAL_ECHOLNPAIR(" External light diagnostic override: ", leds::external_light_bar::diagnostic_override_active() ? (leds::external_light_bar::diagnostic_override_on() ? "on" : "off") : "none");
    SERIAL_ECHOLNPAIR(" IO expander config: ", config_register, " output: ", output_register, " input: ", input_register);
}
#endif

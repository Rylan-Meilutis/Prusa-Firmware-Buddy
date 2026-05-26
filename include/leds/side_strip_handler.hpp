#pragma once

#include <utils/led_color.hpp>
#include "printers.h"
#include "dimming_enabled.hpp"
#include "light_state.hpp"

#include <freertos/mutex.hpp>
#include <optional>
#include <option/has_xbuddy_extension.h>

namespace leds {

enum class SideStripState {
    unknown,
    off,
    dimmed,
    active,
    printing,
    idle,
    custom_color,
    _last = custom_color,
};

class SideStripHandler {
public:
    static constexpr bool has_white_led_and_enclosure_on_second_driver() {
#if PRINTER_IS_PRUSA_XL()
        return true;
#else
        return false;
#endif
    }

    static constexpr bool has_white_led() {
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
        return true;
#else
        return has_white_led_and_enclosure_on_second_driver();
#endif
    }

    static SideStripHandler &instance();

    SideStripHandler();

    void activity_ping();
    void event_ping();
    void idle_ping();
    void print_finished_ping();
    void set_door_open(bool open, uint16_t raw_data);

    void set_custom_color(ColorRGBW color, uint32_t duration_ms, uint32_t transition_ms);

    void update();

    void load_config();

    /// Set maximum brightness of the side leds white channel. The W brightness is limited to this value.
    /// 0 = disable side leds completely (even RGB status blinking), 255 = full
    uint8_t get_max_brightness() const;
    void set_max_brightness(uint8_t value);
    /// Set brightness of the side leds while the leds are dimmed down.
    // /// 0 = disable dimmed side leds, 255 = full brightness
    uint8_t get_dimmed_brightness() const;
    void set_dimmed_brightness(uint8_t value);

    DimmingEnabled get_dimming_enabled() const;
    void set_dimming_enabled(DimmingEnabled value);
    bool get_main_light_enabled(LightState state) const;
    void set_main_light_enabled(LightState state, bool value);
    uint8_t get_brightness(LightState state) const;
    void set_brightness(LightState state, uint8_t value);
    bool get_door_holds_active() const;
    void set_door_holds_active(bool value);

    uint16_t get_activity_timeout_s() const;
    void set_activity_timeout_s(uint16_t value);
    uint16_t get_event_timeout_s() const;
    void set_event_timeout_s(uint16_t value);
    uint16_t get_off_timeout_s() const;
    void set_off_timeout_s(uint16_t value);

    bool get_post_print_hold_enabled() const;
    void set_post_print_hold_enabled(bool value);
    bool post_print_hold_active() const;
    bool post_print_status_dismissed() const;

    uint8_t get_print_brightness() const;
    void set_print_brightness(uint8_t value);
    bool get_print_light_enabled() const;
    void set_print_light_enabled(bool value);

    bool deep_idle() const;
    bool chamber_light_on() const;
    SideStripState current_state() const;

    leds::ColorRGBW color() const;

private:
    void change_state(SideStripState state);

    ColorRGBW get_color_for_state(SideStripState state) const;

    struct CustomColorState {
        ColorRGBW color;
        uint32_t start_ms;
        uint32_t duration_ms;
        uint32_t transition_ms;
    };

    mutable freertos::Mutex mutex;

    // Values are initialized from config store by load_config() in constructor
    DimmingEnabled dimming_enabled;
    uint8_t max_brightness;
    uint8_t dimmed_brightness;
    uint8_t print_brightness;
    uint8_t deep_idle_brightness;
    uint8_t main_light_state_mask;
    uint16_t activity_timeout_s;
    uint16_t event_timeout_s;
    uint16_t off_timeout_s;
    bool door_holds_active;
    bool post_print_hold_enabled;

    SideStripState state = SideStripState::unknown;
    uint32_t active_timestamp_ms = 0; // Timestamp of the last activity for idle dimming
    bool door_open_for_leds = false;
    uint16_t door_raw_data = 0;
    bool print_or_filter_active_prev = false;
    bool host_idle_override = false;
    bool post_print_hold = false;
    bool post_print_hold_dismissed = false;
    bool post_print_hold_seen_door_open = false;
    bool print_light_disabled = false;
    std::optional<CustomColorState> custom_color;
};

} // namespace leds

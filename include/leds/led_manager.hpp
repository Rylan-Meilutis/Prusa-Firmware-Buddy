#pragma once

#include <utils/timing/rate_limiter.hpp>
#include <option/has_side_leds.h>
#include <cstdint>

namespace leds {

/**
 * @brief A class encapsulating handling of LEDs and other peripherals
 * connected to LED interfaces, like LCD backlight and XL enclosure fan.
 *
 * Takes care of updating the LED states and animations and passing the data
 * over to the Neopixel/ws2812 interfaces.
 */
class LEDManager {
public:
    static LEDManager &instance();

    void init();

    void update();
    void update_lcd_brightness();
    bool lcd_brightness_is_off() const;
#if !HAS_SIDE_LEDS()
    bool wake_lcd_from_dim_idle();
    uint8_t get_print_screen_brightness() const;
    void set_print_screen_brightness(uint8_t brightness);
#endif

    void acknowledge_finished();

    /**
     * @brief Called from the power panic module to quickly turn off leds from the AC fault task.
     */
    void enter_power_panic();

    /**
     * @param brighthess Brightness in percents (1-100)
     */
    void set_lcd_brightness(uint8_t brightness);

private:
    static constexpr uint32_t gui_delay_redraw = 40; // 40 ms => 25 fps
    RateLimiter<uint32_t> rate_limiter { gui_delay_redraw };
    freertos::Mutex power_panic_mutex;
#if !HAS_SIDE_LEDS()
    uint32_t lcd_brightness_wake_until_ms { 0 };
    uint8_t lcd_brightness_wake_percent { 15 };
    bool lcd_brightness_wake_from_print_override { false };
    uint8_t print_screen_brightness_override { 100 };
    bool print_screen_brightness_overridden { false };
    bool print_override_session_active { false };
#endif
    bool power_panic { false };
    bool lcd_brightness_off { false };
};

}; // namespace leds

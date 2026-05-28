// gui.cpp
#include <stdlib.h>
#include <algorithm>

#include "display.hpp"
#include "gui.hpp"
#include "gui_time.hpp" //gui::GetTick
#include "ScreenHandler.hpp"
#include "sound.hpp"
#include "IDialog.hpp"
#include "Jogwheel.hpp"
#include "ScreenShot.hpp"
#include "gui_media_events.hpp"
#include "gui_invalidate.hpp"
#include "knob_event.hpp"
#include "marlin_client.hpp"
#include <utils/timing/rate_limiter.hpp>
#include <logging/log.hpp>
#include "display_hw_checks.hpp"
#include <option/has_leds.h>
#include <guiconfig/guiconfig.h>
#if HAS_LEDS()
    #include <leds/led_manager.hpp>
#elif HAS_ST7789_DISPLAY()
    #include <st7789v.hpp>
    #include <config_store/store_instance.hpp>
    #include <leds/light_state.hpp>
    #include <marlin_server.hpp>
    #include <marlin_vars.hpp>
#endif
#include <option/has_side_leds.h>
#if HAS_SIDE_LEDS()
    #include <leds/side_strip_handler.hpp>
#endif

#include <option/has_touch.h>

#if HAS_TOUCH()
    #include <hw/touchscreen/touchscreen.hpp>
#endif

#include <config_store/store_instance.hpp>

#if HAS_MINI_DISPLAY()
    #include "st7789v.hpp"
#endif

#if HAS_SELFTEST()
    #include <gui/screen_menu_selftest_snake.hpp>
#endif

LOG_COMPONENT_REF(GUI);
LOG_COMPONENT_REF(Touch);

static bool gui_invalid = false;

constexpr padding_ui8_t GuiDefaults::Padding;
constexpr Rect16 GuiDefaults::RectHeader;
constexpr Rect16 GuiDefaults::RectScreenBody;
constexpr Rect16 GuiDefaults::RectScreen;
constexpr Rect16 GuiDefaults::RectScreenNoFoot;
constexpr Rect16 GuiDefaults::RectScreenNoHeader;
constexpr Rect16 GuiDefaults::RectFooter;

static const constexpr uint32_t GUI_DELAY_MIN = 1;
static const constexpr uint32_t GUI_DELAY_MAX = 10;
static const constexpr uint8_t GUI_DELAY_LOOP = 100;
static const constexpr uint32_t GUI_DELAY_REDRAW = 40; // 40 ms => 25 fps

static RateLimiter<uint32_t> gui_roll_timer(txtroll_t::GetBaseTick());
static RateLimiter<uint32_t> gui_loop_timer(GUI_DELAY_LOOP);
static RateLimiter<uint32_t> gui_redraw_timer(GUI_DELAY_REDRAW);
static constexpr uint32_t screen_brightness_wake_ms = 30000;

#if !HAS_LEDS() && HAS_ST7789_DISPLAY()
static uint32_t screen_brightness_wake_since_ms = 0;

static leds::LightState screen_brightness_state() {
    if (screen_brightness_wake_since_ms && ticks_ms() - screen_brightness_wake_since_ms < screen_brightness_wake_ms) {
        return leds::LightState::active;
    }
    screen_brightness_wake_since_ms = 0;

    const marlin_server::State printer_state = marlin_vars().print_state;
    const bool print_active = marlin_server::is_printing_state(printer_state) || marlin_server::serial_print_active();
    return print_active
        ? leds::LightState::printing
        : (printer_state == marlin_server::State::Idle || printer_state == marlin_server::State::Finished || printer_state == marlin_server::State::Exit ? leds::LightState::idle : leds::LightState::active);
}

static bool wake_st7789_from_dim_idle() {
    const auto state = screen_brightness_state();
    const uint8_t brightness = (config_store().screen_brightness_by_state.get() >> leds::light_state_shift(state)) & 0xff;
    if (state != leds::LightState::idle || brightness >= 15) {
        return false;
    }
    screen_brightness_wake_since_ms = ticks_ms();
    st7789v_brightness_enable();
    st7789v_brightness_set((leds::minimum_screen_brightness(leds::LightState::active) * 255) / 100);
    return true;
}
#endif

static bool wake_screen_from_dim_idle() {
#if HAS_SIDE_LEDS()
    return leds::SideStripHandler::instance().wake_screen_from_dim_idle();
#elif HAS_LEDS()
    return leds::LEDManager::instance().wake_lcd_from_dim_idle();
#elif HAS_ST7789_DISPLAY()
    return wake_st7789_from_dim_idle();
#else
    return false;
#endif
}

void gui_init(void) {
    display::init();

// select jogwheel type by measured 'reset delay'
// original displays with 15 position encoder returns values 1-2 (short delay - no capacitor)
// new displays with MK3 encoder returns values around 16000 (long delay - 100nF capacitor)
#if HAS_MINI_DISPLAY()
    // run-time jogwheel type detection decides which type of jogwheel device has (each type has different encoder behaviour)
    jogwheel.SetJogwheelType(st7789v_reset_delay);
#else
    jogwheel.SetJogwheelType(0);
#endif
}

void gui_handle_jogwheel() {
    BtnState_t btn_ev;
    bool is_btn = jogwheel.ConsumeButtonEvent(btn_ev);
    int32_t encoder_diff = jogwheel.ConsumeEncoderDiff();

    if (encoder_diff != 0 || is_btn) {
        if (wake_screen_from_dim_idle()) {
            return;
        }
#if HAS_SIDE_LEDS() && PRINTER_IS_PRUSA_XL()
        leds::SideStripHandler::instance().activity_ping();
#endif
        gui::knob::EventEncoder(encoder_diff);

        if (is_btn) {
            gui::knob::EventClick(btn_ev);
        }
    }
}

#if HAS_TOUCH()
void gui_handle_touch() {
    if (!touchscreen.is_enabled()) {
        return;
    }

    const auto touch_event = touchscreen.get_event();
    if (!touch_event) {
        return;
    }

    if (wake_screen_from_dim_idle()) {
        return;
    }

    // we clicked on something, does not really matter on what we clicked
    // we must notify serve to so it knows user is doing something and resets menu timeout, heater timeout ...
    Screens::Access()->ResetTimeout();
#if HAS_SIDE_LEDS() && PRINTER_IS_PRUSA_XL()
    leds::SideStripHandler::instance().activity_ping();
#endif

    if (touch_event.type == GUI_event_t::TOUCH_CLICK) {
        Sound_Play(eSOUND_TYPE::ButtonEcho);
        marlin_client::notify_server_about_knob_click();
    }

    event_conversion_union event_data {
        .point = {
            .x = touch_event.pos_x,
            .y = touch_event.pos_y,
        }
    };

    // Determine if we should propagate the event only to the captured window or globally as a screen event
    const bool propagate_as_screen_event = (touch_event.type != GUI_event_t::TOUCH_CLICK);

    if (propagate_as_screen_event) {
        Screens::Access()->ScreenEvent(nullptr, touch_event.type, event_data.pvoid);
    }

    else if (window_t *captured_window = Screens::Access()->Get()->GetCapturedWindow(); captured_window && captured_window->get_rect_for_touch().Contain(event_data.point)) {
        captured_window->WindowEvent(captured_window, touch_event.type, event_data.pvoid);
    }
}
#endif

void gui_redraw(void) {
    const uint32_t now = ticks_ms();

    if (gui_loop_timer.check(now)) {
        Screens::Access()->ScreenEvent(nullptr, GUI_event_t::LOOP, 0);
    }

    if (txtroll_t::HasInstance() && gui_roll_timer.check(now)) {
        Screens::Access()->ScreenEvent(nullptr, GUI_event_t::TEXT_ROLL, nullptr);
    }

    bool should_sleep = true;
    if (gui_invalid) {
        if (gui_redraw_timer.check(now)) {
            Screens::Access()->Draw();
            gui_invalid = false;
            should_sleep = false;
        }
    }

    if (should_sleep) {
        uint32_t sleep = std::clamp(gui_redraw_timer.remaining_cooldown(now), GUI_DELAY_MIN, GUI_DELAY_MAX);
        osDelay(sleep);
    }
}

// at least one window is invalid
void gui_invalidate(void) {
    gui_invalid = true;
}

static uint8_t guiloop_nesting = 0;
uint8_t gui_get_nesting(void) { return guiloop_nesting; }

void gui_bare_loop() {
    ++guiloop_nesting;

    gui_handle_jogwheel();

    gui_redraw();

    --guiloop_nesting;
}

void gui_loop(void) {
    ++guiloop_nesting;
    lcd::communication_check();
    gui_handle_jogwheel();

#if HAS_LEDS()
    leds::LEDManager::instance().update();
#elif HAS_ST7789_DISPLAY()
    const leds::LightState state = screen_brightness_state();
    const uint8_t stored_brightness = (config_store().screen_brightness_by_state.get() >> leds::light_state_shift(state)) & 0xff;
    const uint8_t brightness = leds::clamp_screen_brightness(state, stored_brightness);
    st7789v_brightness_enable();
    st7789v_brightness_set((brightness * 255) / 100);
#endif

#if HAS_TOUCH()
    gui_handle_touch();
#endif

    MediaState_t media_state = MediaState_t::unknown;
    if (GuiMediaEventsHandler::ConsumeSent(media_state)) {
        switch (media_state) {
        case MediaState_t::inserted:
        case MediaState_t::removed:
        case MediaState_t::error:
            Screens::Access()->ScreenEvent(nullptr, GUI_event_t::MEDIA, (void *)int(media_state));
            break;
        default:
            break;
        }
    }

    gui_redraw();
    marlin_client::loop();
    GuiMediaEventsHandler::Tick();
#if HAS_SELFTEST()
    if (marlin_client::event_clr(marlin_server::Event::RequestCalibrationsScreen)) {
        Screens::Access()->Open<ScreenMenuSTSCalibrations>();
    }
#endif
    --guiloop_nesting;
}

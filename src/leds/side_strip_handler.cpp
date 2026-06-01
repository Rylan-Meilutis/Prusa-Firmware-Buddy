#include "leds/side_strip_handler.hpp"
#include <led_animation_controller/simple_transition_controller.hpp>
#include "marlin_server.hpp"
#include <marlin_vars.hpp>
#include <option/has_chamber_filtration_api.h>
#include <algorithm>

#if HAS_CHAMBER_FILTRATION_API()
    #include <feature/chamber_filtration/chamber_filtration.hpp>
#endif

namespace leds {

static SimpleTransitionController &controller_instance() {
    static SimpleTransitionController instance;
    return instance;
}

static bool print_active_for_leds() {
    return marlin_server::is_printing_state(marlin_vars().print_state.get()) || marlin_server::serial_print_active();
}

static bool guided_activity_active() {
    return marlin_vars().peek_fsm_states([](const fsm::States &states) {
        return states.get_top().has_value();
    });
}

static LightState light_state_for_strip_state(SideStripState state) {
    switch (state) {
    case SideStripState::printing:
        return LightState::printing;
    case SideStripState::active:
    case SideStripState::custom_color:
        return LightState::active;
    case SideStripState::dimmed:
    case SideStripState::idle:
        return LightState::idle;
    case SideStripState::off:
    case SideStripState::unknown:
        return LightState::deep_idle;
    }
    return LightState::deep_idle;
}

static uint8_t packed_screen_brightness(uint32_t values, LightState state) {
    return clamp_screen_brightness(state, (values >> light_state_shift(state)) & 0xff);
}

SideStripHandler &SideStripHandler::instance() {
    static SideStripHandler instance;
    return instance;
}

SideStripHandler::SideStripHandler() {
    load_config();
}

void SideStripHandler::activity_ping() {
    std::lock_guard lock(mutex);
    host_idle_override = false;
    active_timestamp_ms = ticks_ms();
}

void SideStripHandler::event_ping() {
    std::lock_guard lock(mutex);
    host_idle_override = false;
    active_timestamp_ms = ticks_ms();
}

void SideStripHandler::idle_ping() {
    std::lock_guard lock(mutex);
    host_idle_override = true;
    if (!door_open_for_leds) {
        post_print_hold = false;
        post_print_hold_dismissed = true;
        post_print_hold_seen_door_open = false;
        active_timestamp_ms = 0;
        state = SideStripState::unknown;
    }
}

void SideStripHandler::print_finished_ping() {
    std::lock_guard lock(mutex);
    host_idle_override = false;
    print_or_filter_active_prev = false;
    post_print_hold_seen_door_open = door_open_for_leds;
    post_print_hold_dismissed = false;
    if (post_print_hold_enabled) {
        post_print_hold = true;
        active_timestamp_ms = ticks_ms();
        state = SideStripState::unknown;
    } else {
        post_print_hold = false;
        active_timestamp_ms = 0;
    }
}

void SideStripHandler::set_door_open(bool open, uint16_t raw_data) {
    std::lock_guard lock(mutex);

    if (door_open_for_leds == open) {
        door_raw_data = raw_data;
        return;
    }

    const bool was_open = door_open_for_leds;
    door_open_for_leds = open;
    door_raw_data = raw_data;

    if (post_print_hold) {
        if (open) {
            post_print_hold_seen_door_open = true;
        } else if (was_open && post_print_hold_seen_door_open) {
            post_print_hold = false;
            post_print_hold_dismissed = true;
            post_print_hold_seen_door_open = false;
            active_timestamp_ms = 0;
            state = SideStripState::unknown;
        }
    }

    if (open || was_open) {
        host_idle_override = false;
        active_timestamp_ms = ticks_ms();
    }
}

void SideStripHandler::set_custom_color(ColorRGBW color, uint32_t duration_ms, uint32_t transition_ms) {
    std::lock_guard lock(mutex);
    custom_color.emplace(color, ticks_ms(), duration_ms, transition_ms);
    // Set state temporarily to off to force a change in case the state is already custom_color
    state = SideStripState::off;
}

void SideStripHandler::update() {
    std::lock_guard lock(mutex);
    const uint32_t time_ms = ticks_ms();

    auto &controller = controller_instance();
    if (custom_color && (custom_color->duration_ms == 0 || time_ms - custom_color->start_ms < custom_color->duration_ms)) {
        change_state(SideStripState::custom_color);
    } else {
        custom_color.reset();

        const bool print_active = print_active_for_leds() && !host_idle_override;
        // A finished-print screen may remain open while post-print filtration runs.
        // It must not keep the chamber lights and LCD awake indefinitely.
        const bool guided_activity = guided_activity_active() && !host_idle_override && !marlin_server::finishing_or_finished();

        if (print_active) {
            print_or_filter_active_prev = true;
            post_print_hold = false;
            post_print_hold_dismissed = false;
            post_print_hold_seen_door_open = false;
            if (print_light_disabled) {
                change_state(SideStripState::off);
                controller.update();
                return;
            } else {
                change_state(SideStripState::printing);
            }

        } else {
            print_light_disabled = false;
            print_brightness_overridden = false;
            print_screen_brightness_overridden = false;
            if (print_or_filter_active_prev && marlin_vars().print_state == marlin_server::State::Finishing_WaitIdle) {
                active_timestamp_ms = time_ms;
            } else if (print_or_filter_active_prev && marlin_server::finishing_or_finished() && post_print_hold_enabled) {
                print_or_filter_active_prev = false;
                post_print_hold = true;
                post_print_hold_dismissed = false;
                post_print_hold_seen_door_open = door_open_for_leds;
                active_timestamp_ms = time_ms;
            } else if (print_or_filter_active_prev && marlin_server::finishing_or_finished()) {
                print_or_filter_active_prev = false;
                active_timestamp_ms = 0;
            } else if (print_or_filter_active_prev && !marlin_server::finishing_or_finished()) {
                print_or_filter_active_prev = false;
                active_timestamp_ms = time_ms;
            }

            if (guided_activity) {
                active_timestamp_ms = time_ms;
                change_state(SideStripState::active);
            } else if (door_open_for_leds && door_holds_active) {
                change_state(SideStripState::active);
            } else if (active_timestamp_ms == 0) {
                change_state(SideStripState::off);
                controller.update();
                return;
            } else {
                const uint32_t elapsed_ms = time_ms - active_timestamp_ms;
                const uint32_t off_timeout_ms = static_cast<uint32_t>(off_timeout_s) * 1000;
                const uint32_t dim_timeout_ms = static_cast<uint32_t>(activity_timeout_s) * 1000;

                if (off_timeout_ms > 0 && elapsed_ms >= dim_timeout_ms && elapsed_ms - dim_timeout_ms >= off_timeout_ms) {
                    active_timestamp_ms = 0;
                    change_state(SideStripState::off);
                } else if (elapsed_ms >= dim_timeout_ms) {
                    change_state(SideStripState::dimmed);
                } else {
                    change_state(SideStripState::active);
                }
            }
        }
    }

    controller.update();
}

void SideStripHandler::load_config() {
    std::lock_guard lock(mutex);
    dimmed_brightness = config_store().side_leds_dimmed_brightness.get();
    print_brightness = config_store().side_leds_print_brightness.get();
    max_brightness = config_store().side_leds_max_brightness.get();
    deep_idle_brightness = config_store().side_leds_deep_idle_brightness.get();
    dimming_enabled = config_store().side_leds_dimming_enabled.get();
    main_light_state_mask = config_store().side_leds_state_mask.get();
    activity_timeout_s = config_store().side_leds_activity_timeout_s.get();
    event_timeout_s = config_store().side_leds_event_timeout_s.get();
    off_timeout_s = config_store().side_leds_off_timeout_s.get();
    door_holds_active = config_store().side_leds_door_holds_active.get();
    post_print_hold_enabled = config_store().post_print_led_hold_enabled.get();
    // Set state to off to force a change of state that will transition to the new brightness
    state = SideStripState::unknown;
}

uint8_t SideStripHandler::get_max_brightness() const {
    std::lock_guard lock(mutex);
    return max_brightness;
}

void SideStripHandler::set_max_brightness(uint8_t value) {
    config_store().side_leds_max_brightness.set(value);
    std::lock_guard lock(mutex);
    if (max_brightness == value) {
        return;
    }

    max_brightness = value;
    // Force a state refresh so the current state's color is recalculated.
    state = SideStripState::unknown;
}

uint8_t SideStripHandler::get_dimmed_brightness() const {
    std::lock_guard lock(mutex);
    return dimmed_brightness;
}

void SideStripHandler::set_dimmed_brightness(uint8_t value) {
    config_store().side_leds_dimmed_brightness.set(value);
    std::lock_guard lock(mutex);
    if (dimmed_brightness == value) {
        return;
    }

    dimmed_brightness = value;
    // Force a state refresh so the current state's color is recalculated.
    state = SideStripState::unknown;
}

DimmingEnabled SideStripHandler::get_dimming_enabled() const {
    std::lock_guard lock(mutex);
    return dimming_enabled;
}

void SideStripHandler::set_dimming_enabled(DimmingEnabled value) {
    config_store().side_leds_dimming_enabled.set(value);
    std::lock_guard lock(mutex);
    dimming_enabled = value;
}

bool SideStripHandler::get_main_light_enabled(LightState state) const {
    std::lock_guard lock(mutex);
    return main_light_state_mask & light_state_bit(state);
}

void SideStripHandler::set_main_light_enabled(LightState state, bool value) {
    std::lock_guard lock(mutex);
    const uint8_t bit = light_state_bit(state);
    if (value) {
        main_light_state_mask |= bit;
    } else {
        main_light_state_mask &= ~bit;
    }
    config_store().side_leds_state_mask.set(main_light_state_mask);
    this->state = SideStripState::unknown;
}

uint8_t SideStripHandler::get_brightness(LightState state) const {
    std::lock_guard lock(mutex);
    switch (state) {
    case LightState::deep_idle:
        return deep_idle_brightness;
    case LightState::idle:
        return dimmed_brightness;
    case LightState::active:
        return max_brightness;
    case LightState::printing:
        return print_brightness;
    }
    return 0;
}

void SideStripHandler::set_brightness(LightState state, uint8_t value) {
    std::lock_guard lock(mutex);

    switch (state) {
    case LightState::deep_idle:
        config_store().side_leds_deep_idle_brightness.set(value);
        deep_idle_brightness = value;
        break;
    case LightState::idle:
        config_store().side_leds_dimmed_brightness.set(value);
        dimmed_brightness = value;
        break;
    case LightState::active:
        config_store().side_leds_max_brightness.set(value);
        max_brightness = value;
        break;
    case LightState::printing:
        config_store().side_leds_print_brightness.set(value);
        print_brightness = value;
        break;
    }
    this->state = SideStripState::unknown;
}

uint8_t SideStripHandler::get_screen_brightness(LightState state) const {
    return packed_screen_brightness(config_store().screen_brightness_by_state.get(), state);
}

void SideStripHandler::set_screen_brightness(LightState state, uint8_t value) {
    value = clamp_screen_brightness(state, value);

    const uint8_t shift = light_state_shift(state);
    const uint32_t stored = config_store().screen_brightness_by_state.get();
    config_store().screen_brightness_by_state.set((stored & ~(0xffu << shift)) | (static_cast<uint32_t>(value) << shift));
}

uint8_t SideStripHandler::current_screen_brightness() const {
    std::lock_guard lock(mutex);
    const LightState light_state = light_state_for_strip_state(state);
    if (light_state == LightState::printing && print_screen_brightness_overridden) {
        return print_screen_brightness_override;
    }
    return packed_screen_brightness(config_store().screen_brightness_by_state.get(), light_state);
}

bool SideStripHandler::wake_screen_from_dim_idle() {
    std::lock_guard lock(mutex);
    const LightState light_state = light_state_for_strip_state(state);
    const bool dim_idle = (light_state == LightState::idle || light_state == LightState::deep_idle)
        && packed_screen_brightness(config_store().screen_brightness_by_state.get(), light_state) < 15;
    if (dim_idle) {
        host_idle_override = false;
        active_timestamp_ms = ticks_ms();
    }
    return dim_idle;
}

bool SideStripHandler::get_door_holds_active() const {
    std::lock_guard lock(mutex);
    return door_holds_active;
}

void SideStripHandler::set_door_holds_active(bool value) {
    config_store().side_leds_door_holds_active.set(value);
    std::lock_guard lock(mutex);
    door_holds_active = value;
    if (!door_holds_active && door_open_for_leds) {
        active_timestamp_ms = ticks_ms();
    }
}

uint16_t SideStripHandler::get_activity_timeout_s() const {
    std::lock_guard lock(mutex);
    return activity_timeout_s;
}

void SideStripHandler::set_activity_timeout_s(uint16_t value) {
    config_store().side_leds_activity_timeout_s.set(value);
    std::lock_guard lock(mutex);
    activity_timeout_s = value;
}

uint16_t SideStripHandler::get_event_timeout_s() const {
    std::lock_guard lock(mutex);
    return event_timeout_s;
}

void SideStripHandler::set_event_timeout_s(uint16_t value) {
    config_store().side_leds_event_timeout_s.set(value);
    std::lock_guard lock(mutex);
    event_timeout_s = value;
}

uint16_t SideStripHandler::get_off_timeout_s() const {
    std::lock_guard lock(mutex);
    return off_timeout_s;
}

void SideStripHandler::set_off_timeout_s(uint16_t value) {
    config_store().side_leds_off_timeout_s.set(value);
    std::lock_guard lock(mutex);
    off_timeout_s = value;
}

bool SideStripHandler::get_post_print_hold_enabled() const {
    std::lock_guard lock(mutex);
    return post_print_hold_enabled;
}

void SideStripHandler::set_post_print_hold_enabled(bool value) {
    config_store().post_print_led_hold_enabled.set(value);
    std::lock_guard lock(mutex);
    post_print_hold_enabled = value;
    if (!post_print_hold_enabled) {
        post_print_hold = false;
        post_print_hold_dismissed = false;
        post_print_hold_seen_door_open = false;
    }
}

bool SideStripHandler::post_print_hold_active() const {
    std::lock_guard lock(mutex);
    return post_print_hold && state != SideStripState::off;
}

bool SideStripHandler::post_print_status_dismissed() const {
    std::lock_guard lock(mutex);
    return post_print_hold_dismissed;
}

uint8_t SideStripHandler::get_print_brightness() const {
    std::lock_guard lock(mutex);
    return print_brightness;
}

void SideStripHandler::set_print_brightness(uint8_t value) {
    config_store().side_leds_print_brightness.set(value);
    std::lock_guard lock(mutex);
    print_brightness = value;
    state = SideStripState::unknown;
}

uint8_t SideStripHandler::get_print_light_brightness() const {
    std::lock_guard lock(mutex);
    return print_brightness_overridden ? print_brightness_override : print_brightness;
}

void SideStripHandler::set_print_light_brightness(uint8_t value) {
    std::lock_guard lock(mutex);
    print_brightness_override = value;
    print_brightness_overridden = true;
    print_light_disabled = value == 0;
    host_idle_override = false;
    state = SideStripState::unknown;
}

uint8_t SideStripHandler::get_print_screen_brightness() const {
    std::lock_guard lock(mutex);
    if (print_screen_brightness_overridden) {
        return print_screen_brightness_override;
    }
    return packed_screen_brightness(config_store().screen_brightness_by_state.get(), LightState::printing);
}

void SideStripHandler::set_print_screen_brightness(uint8_t value) {
    std::lock_guard lock(mutex);
    print_screen_brightness_override = clamp_screen_brightness(LightState::printing, value);
    print_screen_brightness_overridden = true;
    host_idle_override = false;
    state = SideStripState::unknown;
}

bool SideStripHandler::deep_idle() const {
    std::lock_guard lock(mutex);
    return state == SideStripState::off;
}

LightState SideStripHandler::current_light_state() const {
    std::lock_guard lock(mutex);
    return light_state_for_strip_state(state);
}

bool SideStripHandler::chamber_light_on() const {
    std::lock_guard lock(mutex);
    return get_color_for_state(state).w > 0;
}

SideStripState SideStripHandler::current_state() const {
    std::lock_guard lock(mutex);
    return state;
}

leds::ColorRGBW SideStripHandler::color() const {
    std::lock_guard lock(mutex);
    return controller_instance().color();
}

void SideStripHandler::change_state(SideStripState state) {
    if (this->state != state) {
        this->state = state;

        uint32_t duration;
        if (state == SideStripState::dimmed || state == SideStripState::idle) {
            duration = 5000;
        } else if (state == SideStripState::custom_color) {
            duration = custom_color->transition_ms;
        } else {
            duration = 500;
        }
        controller_instance().set(get_color_for_state(state), duration);
    }
}

ColorRGBW SideStripHandler::get_color_for_state(SideStripState state) const {
    constexpr auto base_color = has_white_led() ? ColorRGBW(0, 0, 0, 255) : ColorRGBW(255, 255, 255);

    const LightState light_state = light_state_for_strip_state(state);
    const bool print_override_active = light_state == LightState::printing && print_brightness_overridden;
    if (!(main_light_state_mask & light_state_bit(light_state)) && !print_override_active) {
        return ColorRGBW();
    }

    switch (state) {
    case SideStripState::unknown:
        return ColorRGBW();
    case SideStripState::off:
        return base_color.clamp(deep_idle_brightness);
    case SideStripState::dimmed:
        return base_color.clamp(dimmed_brightness);
    case SideStripState::printing:
        return base_color.clamp(print_brightness_overridden ? print_brightness_override : print_brightness);
    case SideStripState::active:
        return base_color.clamp(max_brightness);
    case SideStripState::idle:
        return base_color.clamp(max_brightness);
    case SideStripState::custom_color:
        return ColorRGBW(custom_color->color);
    }
    return {};
}

} // namespace leds

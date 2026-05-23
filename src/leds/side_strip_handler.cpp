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
    return marlin_server::is_printing_state(marlin_vars().print_state.get());
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
    active_timestamp_ms = ticks_ms();
}

void SideStripHandler::event_ping() {
    std::lock_guard lock(mutex);
    active_timestamp_ms = ticks_ms();
}

void SideStripHandler::print_finished_ping() {
    std::lock_guard lock(mutex);
    print_or_filter_active_prev = false;
    post_print_hold_seen_door_open = false;
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
            active_timestamp_ms = ticks_ms();
            state = SideStripState::unknown;
        }
    }

    if (open || was_open) {
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

        const bool print_active = print_active_for_leds();

        if (print_active) {
            print_or_filter_active_prev = true;
            post_print_hold = false;
            post_print_hold_dismissed = false;
            post_print_hold_seen_door_open = false;
            change_state(SideStripState::printing);

        } else {
            if (print_or_filter_active_prev && marlin_vars().print_state == marlin_server::State::Finishing_WaitIdle) {
                active_timestamp_ms = time_ms;
            } else if (print_or_filter_active_prev && marlin_server::finishing_or_finished() && post_print_hold_enabled) {
                print_or_filter_active_prev = false;
                post_print_hold = true;
                post_print_hold_dismissed = false;
                post_print_hold_seen_door_open = false;
                active_timestamp_ms = time_ms;
            } else if (print_or_filter_active_prev && marlin_server::finishing_or_finished()) {
                print_or_filter_active_prev = false;
                active_timestamp_ms = 0;
            } else if (print_or_filter_active_prev && !marlin_server::finishing_or_finished()) {
                print_or_filter_active_prev = false;
                active_timestamp_ms = time_ms;
            }

            if (door_open_for_leds) {
                change_state(SideStripState::active);
            } else if (active_timestamp_ms == 0) {
                change_state(SideStripState::off);
                controller.update();
                return;
            } else {
                const uint32_t elapsed_ms = time_ms - active_timestamp_ms;
                const uint32_t off_timeout_ms = static_cast<uint32_t>(off_timeout_s) * 1000;
                uint32_t dim_timeout_ms = static_cast<uint32_t>(activity_timeout_s) * 1000;

                if (off_timeout_ms > 0) {
                    dim_timeout_ms = std::min(dim_timeout_ms, off_timeout_ms);
                }

                if (off_timeout_ms > 0 && elapsed_ms >= off_timeout_ms) {
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
    dimming_enabled = config_store().side_leds_dimming_enabled.get();
    activity_timeout_s = config_store().side_leds_activity_timeout_s.get();
    event_timeout_s = config_store().side_leds_event_timeout_s.get();
    off_timeout_s = config_store().side_leds_off_timeout_s.get();
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

bool SideStripHandler::deep_idle() const {
    std::lock_guard lock(mutex);
    return state == SideStripState::off;
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

ColorRGBW SideStripHandler::get_color_for_state(SideStripState state) {
    constexpr auto base_color = has_white_led() ? ColorRGBW(0, 0, 0, 255) : ColorRGBW(255, 255, 255);

    switch (state) {
    case SideStripState::off:
    case SideStripState::unknown:
        return ColorRGBW();
    case SideStripState::dimmed:
        return base_color.clamp(dimmed_brightness);
    case SideStripState::printing:
        return base_color.clamp(std::max(print_brightness, max_brightness));
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

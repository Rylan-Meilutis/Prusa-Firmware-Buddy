/// @file
#include "indx_head_leds.hpp"

#include <puppies/INDX.hpp>
#include <state/printer_state.hpp>
#include <module/prusa/toolchanger.h>
#include <config_store/store_instance.hpp>
#include <utils/color.hpp>

#include <cstdint>
#include <optional>

namespace indx_head_leds {

namespace {

    using printer_state::DeviceState;

    /// What the head LEDs currently express, ordered by priority (highest wins in compute_state).
    enum class HeadLedState {
        disabled, ///< tool LEDs turned off by the user
        idle, ///< green solid
        busy, ///< blue solid (active operation without heating, e.g. moving with no tool)
        heating, ///< temperature gradient animation (blue -> yellow -> red)
        warning, ///< blinking orange (user action needed)
    };

    // Product-defined palette / timing — tweak here.
    constexpr Color color_idle = Color::from_rgb(0, 255, 0);
    constexpr Color color_busy = Color::from_rgb(0, 150, 255); // matches the buddy status strip's "printing" blue
    constexpr Color color_warning = COLOR_ORANGE;
    constexpr Color color_off = Color::from_rgb(0, 0, 0);
    constexpr uint16_t static_fade_delay_ms = 500; ///< fade time for solid colors (idle, busy)
    constexpr uint16_t warning_blink_interval_ms = 500; ///< head alternates every interval -> 1 s full cycle
    constexpr float hot_nozzle_threshold_c = 50.0f; ///< a nozzle hotter than this keeps the gradient, not idle-green

    bool is_hot_or_heating() {
        // Heating (target set) or still hot and cooling — a hot nozzle must not read as idle-green.
        return buddy::puppies::indx.get_hotend_target_temp() > 0
            || buddy::puppies::indx.get_hotend_temp_compensated() > hot_nozzle_threshold_c;
    }

    HeadLedState compute_state() {
        if (!config_store().tool_leds_enabled.get()) {
            return HeadLedState::disabled;
        }

        const DeviceState ds = printer_state::get_state();

        // User action needed wins over everything (product priority: warning > heating).
        if (ds == DeviceState::Attention) {
            return HeadLedState::warning;
        }

        // A hot / heating tool shows the temperature gradient, even mid-print.
        if (is_hot_or_heating()) {
            return HeadLedState::heating;
        }

        // Toolchange moves raise no busy FSM of their own — keep the head blue throughout.
        if (prusa_toolchanger.phase() != PrusaToolChanger::ToolchangePhase::none) {
            return HeadLedState::busy;
        }

        // Any other active operation reported by the printer state.
        switch (ds) {
        case DeviceState::Busy:
        case DeviceState::Printing:
        case DeviceState::Paused:
            return HeadLedState::busy;
        default:
            return HeadLedState::idle;
        }
    }

    void apply_state(HeadLedState state) {
        auto &indx = buddy::puppies::indx;
        switch (state) {
        case HeadLedState::disabled:
            indx.set_leds_enabled(false);
            break;
        case HeadLedState::idle:
            indx.set_leds_solid_color(color_idle, static_fade_delay_ms);
            break;
        case HeadLedState::busy:
            indx.set_leds_solid_color(color_busy, static_fade_delay_ms);
            break;
        case HeadLedState::heating:
            indx.set_leds_to_follow_nozle_temp();
            break;
        case HeadLedState::warning:
            indx.set_leds_blinking(color_warning, color_off, warning_blink_interval_ms);
            break;
        }
    }

} // namespace

void update() {
    // LEDManager::update() already rate-limits us; only push to the head on a real change.
    static std::optional<HeadLedState> last_state;

    const HeadLedState state = compute_state();
    if (last_state == state) {
        return;
    }
    last_state = state;
    apply_state(state);
}

} // namespace indx_head_leds

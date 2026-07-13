/// @file
#include "indx_head_leds.hpp"

#include <puppies/INDX.hpp>
#include <leds/status_leds_handler.hpp>
#include <indx_head/leds.hpp>
#include <config_store/store_instance.hpp>
#include <utils/color.hpp>
#include <utils/enum_array.hpp>

#include <cstdint>
#include <optional>

namespace indx_head_leds {

namespace {

    using indx_head::leds::Mode;
    using leds::StateAnimation;

    struct LedSetting {
        Color color;
        Mode mode;
        uint16_t period_ms;
    };

    constexpr EnumArray<StateAnimation, LedSetting, static_cast<int>(StateAnimation::_last) + 1> palette {
        { StateAnimation::Idle, { .color = Color::from_rgb(0, 0, 0), .mode = Mode::solid, .period_ms = 500 } },
        { StateAnimation::Printing, { .color = Color::from_rgb(0, 150, 255), .mode = Mode::solid, .period_ms = 500 } },
        { StateAnimation::Finished, { .color = Color::from_rgb(0, 255, 0), .mode = Mode::solid, .period_ms = 500 } },
        { StateAnimation::Aborted, { .color = Color::from_rgb(0, 0, 0), .mode = Mode::solid, .period_ms = 500 } },
        { StateAnimation::Warning, { .color = Color::from_rgb(255, 255, 0), .mode = Mode::pulsing, .period_ms = 1000 } },
        { StateAnimation::PowerPanic, { .color = Color::from_rgb(0, 0, 0), .mode = Mode::solid, .period_ms = 500 } },
        { StateAnimation::PowerUp, { .color = Color::from_rgb(0, 255, 0), .mode = Mode::pulsing, .period_ms = 1500 } },
        { StateAnimation::Error, { .color = Color::from_rgb(255, 0, 0), .mode = Mode::pulsing, .period_ms = 500 } },
    };

    constexpr Color color_off = Color::from_rgb(0, 0, 0);

    std::optional<StateAnimation> compute_state() {
        if (!config_store().tool_leds_enabled.get()) {
            return std::nullopt;
        }
        return leds::StatusLedsHandler::instance().current_animation();
    }

    void apply_state(const std::optional<StateAnimation> &state) {
        auto &indx = buddy::puppies::indx;
        if (!state.has_value()) {
            indx.set_leds_enabled(false);
            return;
        }

        const LedSetting &setting = palette[*state];
        switch (setting.mode) {
        case Mode::off:
            indx.set_leds_enabled(false);
            break;
        case Mode::solid:
            indx.set_leds_solid_color(setting.color, setting.period_ms);
            break;
        case Mode::blinking:
            indx.set_leds_blinking(setting.color, color_off, setting.period_ms);
            break;
        case Mode::pulsing:
            indx.set_leds_pulsing(setting.color, color_off, setting.period_ms);
            break;
        case Mode::match_nozzle_temp:
            indx.set_leds_to_follow_nozle_temp();
            break;
        }
    }

} // namespace

void update() {
    // LEDManager::update() already rate-limits us; only push to the head on a real change.
    static std::optional<StateAnimation> last_state;

    const auto state = compute_state();
    if (last_state == state) {
        return;
    }
    last_state = state;
    apply_state(state);
}

} // namespace indx_head_leds

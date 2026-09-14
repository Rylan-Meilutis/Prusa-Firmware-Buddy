/// @file
#include "indx_head_leds.hpp"

#include <puppies/INDX.hpp>
#include <indx_head/leds.hpp>
#include <config_store/store_instance.hpp>
#include <utils/color.hpp>

#include <algorithm>
#include <cstdint>

namespace indx_head_leds {

namespace {

    struct HeadState {
        bool enabled {};
        Color color {};
        constexpr bool operator==(const HeadState &) const = default;
    };

    constexpr uint8_t rgb_with_white(uint8_t component, uint8_t white) {
        return static_cast<uint8_t>(std::min<uint16_t>(static_cast<uint16_t>(component) + white, 255));
    }

    HeadState compute_state(leds::ColorRGBW front_status_color) {
        if (!config_store().tool_leds_enabled.get()) {
            return {};
        }
        return {
            true,
            Color::from_rgb(
                rgb_with_white(front_status_color.r, front_status_color.w),
                rgb_with_white(front_status_color.g, front_status_color.w),
                rgb_with_white(front_status_color.b, front_status_color.w)),
        };
    }

    void apply_state(const HeadState &state) {
        auto &indx = buddy::puppies::indx;
        if (!state.enabled) {
            indx.set_leds_enabled(false);
            return;
        }
        indx.set_leds_solid_color(state.color, 0);
    }

} // namespace

void update(leds::ColorRGBW front_status_color) {
    // LEDManager::update() already rate-limits us; only push to the head on a real change.
    static HeadState last_state;

    const auto state = compute_state(front_status_color);
    if (last_state == state) {
        return;
    }
    last_state = state;
    apply_state(state);
}

} // namespace indx_head_leds

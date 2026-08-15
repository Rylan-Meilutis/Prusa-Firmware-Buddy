#pragma once

#include <utils/enum_array.hpp>
#include <utils/led_color.hpp>
#include <freertos/timing.hpp>

namespace leds {

template <template <size_t count> typename AnimationType, size_t count>
class AnimationController {
public:
    using AnimationParams = AnimationType<count>::Params;

    AnimationController(const AnimationParams &startup_params)
        : current_animation { startup_params }
        , prev_animation { startup_params } {
    }

    void update() {
        uint32_t time_ms = freertos::millis();

        data_ = current_animation.render();

        float xfade = static_cast<float>(time_ms - current_animation.get_start_time()) / transition_time_ms;
        if (xfade < 1.0f) {
            auto prev_data = prev_animation.render();
            for (size_t i = 0; i < count; ++i) {
                data_[i] = prev_data[i].cross_fade(data_[i], xfade);
            }
        }
    }

    void set(const AnimationParams &params) {
        prev_animation = current_animation;
        current_animation.start(params);
    }

    void set_immediate(const AnimationParams &params) {
        current_animation.start(params);
        prev_animation = current_animation;
    }

    std::span<const ColorRGBW, count> data() const {
        return data_;
    }

private:
    static constexpr uint32_t transition_time_ms { 400 };

    AnimationType<count> current_animation;
    AnimationType<count> prev_animation;

    std::array<ColorRGBW, count> data_;
};

} // namespace leds

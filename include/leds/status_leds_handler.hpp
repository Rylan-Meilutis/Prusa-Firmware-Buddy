#pragma once

#include <utils/led_color.hpp>
#include <led_animation_controller/frame_animation.hpp>
#include <leds/light_state.hpp>

#include <freertos/mutex.hpp>

namespace leds {

enum class StateAnimation : uint8_t {
    Idle,
    Printing,
    Finishing,
    Filtering,
    Aborting,
    Warning,
    PowerPanic,
    PowerUp,
#if PRINTER_IS_PRUSA_iX()
    WaitingForPrinter,
    WaitingForUser,
    Unloading,
    WaitingForFilamentRemoval,
    FilamentRemoved,
    Inserting,
    Loading,
#endif
    Error,
    _last = Error,
};

enum class AnimationType : uint8_t {
#if PRINTER_IS_PRUSA_iX()
    RunningLeft,
    RunningRight,
    PulsingLeft,
    PulsingRight,
    Alternating,
#endif
    Solid,
    Pulsing,
    _last = Pulsing,
};

class StatusLedsHandler {
public:
    static StatusLedsHandler &instance();

    /**
     * @brief Sets an error state, overriding whatever printer state is fetched from Marlin.
     *
     * There's currently no way of returning to a normal state (it's used for BSOD/RSOD).
     */
    void set_error();

    /**
     * @brief Sets current animation, lasting until the next state change.
     */
    void set_animation(StateAnimation state);

    /**
     * @brief Sets a custom animation, lasting until the next state change.
     */
    void set_custom_animation(const ColorRGBW &color, AnimationType type, uint16_t period_ms);

    ColorRGBW get_color() const;
    void reload_colors();

    bool get_active();
    void set_active(bool val);
    bool get_print_status_enabled();
    void set_print_status_enabled(bool val);
    uint8_t get_brightness(LightState state);
    void set_brightness(LightState state, uint8_t val);
    uint16_t get_finished_hold_s();
    void set_finished_hold_s(uint16_t val);
    uint8_t get_print_status_brightness();
    void set_print_status_brightness(uint8_t val);
    void set_idle_light_state(LightState state);
    void acknowledge_finished();
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
    void acknowledge_aborted();
#endif
    void update();

    std::span<const ColorRGBW, 3> led_data();

private:
    freertos::Mutex mutex;
    bool active { config_store().run_leds.get() };
    bool print_status_disabled { false };
    bool print_status_overridden { false };
    uint32_t brightness_by_state { config_store().status_led_brightness_by_state.get() };
    LightState current_light_state { LightState::active };
    uint8_t print_status_brightness { 100 };
    LightState idle_light_state { LightState::active };
    StateAnimation old_state { StateAnimation::Idle };
    bool custom_animation_active { false };
    bool is_error_state { false };
    bool finished_acknowledged { false };
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
    bool aborted_acknowledged { false };
#endif
    bool filtering_prev { false };
    bool finished_prev { false };
    uint32_t finished_hold_until_ms { 0 };
    ColorRGBW old_color {};
    std::array<ColorRGBW, 3> adjusted_data {};

    std::array<FrameAnimation<3>::Params, 2> custom_params_banks;
    uint8_t custom_params_bank_index { 0 };

    ColorRGBW color;
};

} // namespace leds

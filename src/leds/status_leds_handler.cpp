#include "leds/status_leds_handler.hpp"

#include <led_animation_controller/animation_controller.hpp>
#include <led_animation_controller/frame_animation.hpp>
#include "marlin_server.hpp"
#include "marlin_vars.hpp"
#include "client_response.hpp"
#include "timing.h"
#include <option/has_chamber_filtration_api.h>
#include <option/has_side_fsensor.h>
#include <algorithm>

#if HAS_CHAMBER_FILTRATION_API()
    #include <feature/chamber_filtration/chamber_filtration.hpp>
#endif

namespace leds {

using namespace marlin_server;

static bool post_filter_active() {
#if HAS_CHAMBER_FILTRATION_API()
    return marlin_vars().print_state.get() == marlin_server::State::Finished
        && buddy::chamber_filtration().output_pwm().value > 0;
#else
    return false;
#endif
}

static bool print_active_for_status_override() {
    return marlin_server::is_printing_state(marlin_vars().print_state.get()) || marlin_server::serial_print_active();
}

static uint8_t packed_brightness(uint32_t values, LightState state) {
    return (values >> light_state_shift(state)) & 0xff;
}

static StateAnimation marlin_to_anim_state() {
    fsm::States::State load_unload_state;
    bool is_preheating;
    marlin_vars().peek_fsm_states([&](const auto &states) {
        load_unload_state = states[ClientFSM::Load_unload];
        is_preheating = states.is_active(ClientFSM::Preheat);
    });

#if PRINTER_IS_PRUSA_iX()
    /*
     * These comments all refer to states in diagram provided in [BFW-6938]
     */
    if (load_unload_state.has_value()) {
        const PhasesLoadUnload phase = static_cast<PhasesLoadUnload>(load_unload_state->GetPhase());
        switch (phase) {
        // Unloading + waiting for filament removal (there is no way to distinguish between them (extruder fs = in gears))
        case PhasesLoadUnload::Ramming_stoppable:
        case PhasesLoadUnload::Ramming_unstoppable:
        case PhasesLoadUnload::Unloading_stoppable:
        case PhasesLoadUnload::Unloading_unstoppable:
        case PhasesLoadUnload::Ejecting_stoppable:
        case PhasesLoadUnload::Ejecting_unstoppable:
            return StateAnimation::Unloading;
        // Filament in removal
        case PhasesLoadUnload::UnloadNozzleCleaning:
        case PhasesLoadUnload::FilamentNotInFS:
            return StateAnimation::WaitingForFilamentRemoval;
        // Filament Removed
        case PhasesLoadUnload::AwaitingFilament_stoppable:
        case PhasesLoadUnload::AwaitingFilament_unstoppable:
            return StateAnimation::FilamentRemoved;
        // Inserting filament
        case PhasesLoadUnload::Inserting_stoppable:
        case PhasesLoadUnload::Inserting_unstoppable:
            return StateAnimation::Inserting;
        // PreLoading
        case PhasesLoadUnload::LoadingToGears_stoppable:
        case PhasesLoadUnload::LoadingToGears_unstoppable:
    #if HAS_SIDE_FSENSOR()
        case PhasesLoadUnload::LoadingObstruction_stoppable:
        case PhasesLoadUnload::LoadingObstruction_unstoppable:
    #endif
            return StateAnimation::Loading;
        // Loading filament
        case PhasesLoadUnload::Loading_stoppable:
        case PhasesLoadUnload::Loading_unstoppable:
        case PhasesLoadUnload::AutoRetracting:
        case PhasesLoadUnload::Purging_stoppable:
        case PhasesLoadUnload::Purging_unstoppable:
        case PhasesLoadUnload::LoadNozzleCleaning:
            return StateAnimation::Loading;

        case PhasesLoadUnload::WaitingTemp_stoppable:
        case PhasesLoadUnload::WaitingTemp_unstoppable:
        case PhasesLoadUnload::Parking_stoppable:
        case PhasesLoadUnload::Parking_unstoppable:
        case PhasesLoadUnload::Unparking:
        case PhasesLoadUnload::ChangingTool:
            return StateAnimation::WaitingForPrinter;

        case PhasesLoadUnload::IsFilamentUnloaded:
        case PhasesLoadUnload::ManualUnload_continuable:
        case PhasesLoadUnload::ManualUnload_uncontinuable:
        case PhasesLoadUnload::UserPush_stoppable:
        case PhasesLoadUnload::UserPush_unstoppable:
        case PhasesLoadUnload::MakeSureInserted_stoppable:
        case PhasesLoadUnload::MakeSureInserted_unstoppable:
        case PhasesLoadUnload::IsFilamentInGear:
        case PhasesLoadUnload::IsColor:
        case PhasesLoadUnload::IsColorPurge:
            return StateAnimation::WaitingForUser;

        case PhasesLoadUnload::FilamentStuck:
            return StateAnimation::Warning;

        // Other phases, let them be handled by the printer state in next switch
        case PhasesLoadUnload::initial:
        case PhasesLoadUnload::_cnt:
            break;
        }
    }
#endif
    const marlin_server::State printer_state = marlin_vars().print_state;

    switch (printer_state) {
    case State::Idle:
    case State::PrintPreviewInit:
    case State::PrintPreviewImage:
    case State::PrintPreviewConfirmed:
    case State::PrintPreviewQuestions:
#if HAS_TOOLCHANGER() || HAS_MMU2()
    case State::PrintPreviewToolsMapping:
#endif
    case State::Exit:
        return StateAnimation::Idle;

    case State::Printing:
    case State::PrintInit:
    case State::SerialPrintInit:
    case State::Finishing_WaitIdle:
    case State::Pausing_Begin:
    case State::Pausing_Failed_Code:
    case State::Pausing_WaitIdle:
    case State::Pausing_ParkHead:
    case State::Resuming_BufferData:
    case State::Resuming_Begin:
    case State::Resuming_Reheating:
    case State::Resuming_UnparkHead_XY:
    case State::Resuming_UnparkHead_ZE: {
        if (load_unload_state.has_value() || is_preheating) {
            return StateAnimation::Warning;
        } else {
            return StateAnimation::Printing;
        }
    }
    case State::Paused:
    case State::MediaErrorRecovery_BufferData:
        return StateAnimation::Warning;

    case State::Aborting_Begin:
    case State::Aborting_WaitIdle:
    case State::Aborting_ParkHead:
    case State::Aborting_Preview:
    case State::Aborting_UnloadFilament:
    case State::Aborted:
        return StateAnimation::Aborting;

    case State::Finishing_ParkHead:
    case State::Finishing_UnloadFilament:
        return StateAnimation::Finishing;

    case State::Finished:
        return post_filter_active() ? StateAnimation::Filtering : StateAnimation::Finishing;

    case State::CrashRecovery_Begin:
    case State::CrashRecovery_Retracting:
    case State::CrashRecovery_Lifting:
    case State::CrashRecovery_ToolchangePowerPanic:
    case State::CrashRecovery_XY_Measure:
#if HAS_TOOLCHANGER()
    case State::CrashRecovery_Tool_Pickup:
#endif
    case State::CrashRecovery_XY_HOME:
    case State::CrashRecovery_HOMEFAIL:
    case State::CrashRecovery_Axis_NOK:
    case State::CrashRecovery_Repeated_Crash:
        return StateAnimation::Warning;

    case State::PowerPanic_acFault:
    case State::PowerPanic_Resume:
    case State::PowerPanic_AwaitingResume:
        return StateAnimation::PowerPanic;
    }
    return StateAnimation::Idle;
}
namespace {

    using StateAnimationController = AnimationController<FrameAnimation, 3>;

    constexpr auto solid = std::to_array<FrameAnimation<3>::Frame>({ { 100, 100, 100 } });

    constexpr auto pulsing = std::to_array<FrameAnimation<3>::Frame>({ { 100, 100, 100 },
        { 0, 0, 0 } });
#if PRINTER_IS_PRUSA_iX()
    constexpr auto running_left = std::to_array<FrameAnimation<3>::Frame>({ { 0, 0, 100 },
        { 0, 100, 0 }, { 100, 0, 0 } });
    constexpr auto running_right = std::to_array<FrameAnimation<3>::Frame>({ { 100, 0, 0 },
        { 0, 100, 0 }, { 0, 0, 100 } });
    constexpr auto alternating = std::to_array<FrameAnimation<3>::Frame>({ { 100, 0, 100 },
        { 0, 100, 0 } });
    constexpr auto pulsing_left = std::to_array<FrameAnimation<3>::Frame>({ { 100, 0, 0 },
        { 0, 0, 0 } });
    constexpr auto pulsing_right = std::to_array<FrameAnimation<3>::Frame>({ { 0, 0, 100 },
        { 0, 0, 0 } });
#endif
    constexpr EnumArray<StateAnimation, typename FrameAnimation<3>::Params, static_cast<int>(StateAnimation::_last) + 1> animations {
#if PRINTER_IS_PRUSA_iX()
        { StateAnimation::Idle, { { 0, 0, 255 }, 1000, 0, 400, solid } },
            { StateAnimation::Printing, { { 0, 255, 0 }, 1000, 0, 400, solid } },
            { StateAnimation::Finishing, { { 0, 0, 255 }, 500, 0, 250, pulsing } },
#else
        { StateAnimation::Idle, { { 0, 0, 0 }, 1000, 0, 400, solid } },
            { StateAnimation::Printing, { { 75, 46, 131 }, 1000, 0, 400, solid } },
            { StateAnimation::Finishing, { { 0, 255, 0 }, 1000, 0, 400, solid } },
#endif
            { StateAnimation::Filtering, { { 0, 255, 0 }, 2000, 0, 2000, pulsing } },
            { StateAnimation::Aborting, { { 0, 0, 0 }, 1000, 0, 400, solid } },
#if PRINTER_IS_PRUSA_iX()
            { StateAnimation::Warning, { { 128, 32, 0 }, 1000, 0, 1000, pulsing } },
#else
            { StateAnimation::Warning, { { 255, 255, 0 }, 1000, 0, 1000, pulsing } },
#endif
            { StateAnimation::PowerPanic, { { 0, 0, 0 }, 1000, 0, 400, solid } },
            { StateAnimation::PowerUp, { { 0, 255, 0 }, 1500, 0, 1500, pulsing } },
#if PRINTER_IS_PRUSA_iX()
            { StateAnimation::WaitingForPrinter, { { 0, 0, 255 }, 500, 0, 250, alternating } },
            { StateAnimation::WaitingForUser, { { 0, 0, 255 }, 250, 0, 100, alternating } },
            { StateAnimation::Unloading, { { 0, 0, 255 }, 500, 0, 0, pulsing_right } },
            { StateAnimation::WaitingForFilamentRemoval, { { 0, 0, 255 }, 500, 250, 0, running_left } },
            { StateAnimation::FilamentRemoved, { { 0, 0, 255 }, 500, 0, 0, pulsing_left } },
            { StateAnimation::Inserting, { { 0, 0, 255 }, 500, 250, 0, running_right } },
            { StateAnimation::Loading, { { 0, 0, 255 }, 250, 0, 0, running_right } },
#endif
            { StateAnimation::Error, { { 255, 0, 0 }, 500, 0, 500, pulsing } },
    };

    constexpr EnumArray<AnimationType, std::span<const FrameAnimation<3>::Frame>, static_cast<int>(AnimationType::_last) + 1> custom_frames {
#if PRINTER_IS_PRUSA_iX()
        { AnimationType::RunningLeft, running_left },
            { AnimationType::RunningRight, running_right },
            { AnimationType::PulsingLeft, pulsing_left },
            { AnimationType::PulsingRight, pulsing_right },
            { AnimationType::Alternating, alternating },
#endif
            { AnimationType::Solid, solid },
            { AnimationType::Pulsing, pulsing },
    };

    ColorRGBW configured_color(StateAnimation state, ColorRGBW fallback) {
        const auto raw_to_color = [](uint32_t raw) {
            return ColorRGBW(static_cast<uint8_t>((raw >> 16) & 0xff), static_cast<uint8_t>((raw >> 8) & 0xff), static_cast<uint8_t>(raw & 0xff));
        };

        switch (state) {
        case StateAnimation::Idle:
            return raw_to_color(config_store().status_led_idle_color.get());
        case StateAnimation::Printing:
            return raw_to_color(config_store().status_led_printing_color.get());
        case StateAnimation::Finishing:
            return raw_to_color(config_store().status_led_finished_color.get());
        case StateAnimation::Filtering:
            return raw_to_color(config_store().status_led_warning_color.get());
        case StateAnimation::Warning:
        case StateAnimation::PowerPanic:
            return raw_to_color(config_store().status_led_warning_color.get());
        case StateAnimation::Error:
            return raw_to_color(config_store().status_led_error_color.get());
        default:
            return fallback;
        }
    }

} // namespace

StateAnimationController &controller_instance() {
    static StateAnimationController instance { animations[StateAnimation::Idle] };
    return instance;
}

StatusLedsHandler &StatusLedsHandler::instance() {
    static StatusLedsHandler instance;
    return instance;
}

void StatusLedsHandler::set_error() {
    std::lock_guard lock(mutex);
    is_error_state = true;
}

void StatusLedsHandler::set_animation(StateAnimation state) {
    std::lock_guard lock(mutex);
    controller_instance().set(animations[state]);
}

ColorRGBW StatusLedsHandler::get_color() const {
    return color;
}

void StatusLedsHandler::reload_colors() {
    std::lock_guard lock(mutex);
    old_color = {};
    old_state = StateAnimation::_last;
}

bool StatusLedsHandler::get_active() {
    std::lock_guard lock(mutex);
    return active;
}

void StatusLedsHandler::set_custom_animation(const ColorRGBW &color, AnimationType type, uint16_t period_ms) {
    std::lock_guard lock(mutex);
    auto &controller = controller_instance();

    auto &custom_params = custom_params_banks[custom_params_bank_index];

    custom_params.color = color;
    custom_params.frames = custom_frames[type];

    if (period_ms > 0) {
        custom_params.frame_length = period_ms / custom_params.frames.size();
        custom_params.blend_time = custom_params.frame_length / 4;
    } else {
        custom_params.frame_length = 1000;
        custom_params.blend_time = 300;
    }

    controller.set(custom_params);
    custom_params_bank_index = custom_params_bank_index > 0 ? 0 : 1;
}

void StatusLedsHandler::set_active(bool val) {
    std::lock_guard lock(mutex);
    active = val;
    config_store().run_leds.set(val);
}

bool StatusLedsHandler::get_print_status_enabled() {
    std::lock_guard lock(mutex);
    return active && !print_status_disabled && print_status_brightness > 0;
}

uint8_t StatusLedsHandler::get_brightness(LightState state) {
    std::lock_guard lock(mutex);
    return packed_brightness(brightness_by_state, state);
}

void StatusLedsHandler::set_brightness(LightState state, uint8_t val) {
    if (val > 100) {
        val = 100;
    }
    std::lock_guard lock(mutex);
    const uint8_t shift = light_state_shift(state);
    brightness_by_state = (brightness_by_state & ~(0xffu << shift)) | (static_cast<uint32_t>(val) << shift);
    config_store().status_led_brightness_by_state.set(brightness_by_state);
}

uint16_t StatusLedsHandler::get_finished_hold_s() {
    return config_store().status_led_finished_hold_s.get();
}

void StatusLedsHandler::set_finished_hold_s(uint16_t val) {
    config_store().status_led_finished_hold_s.set(val);
}

void StatusLedsHandler::set_print_status_enabled(bool val) {
    std::lock_guard lock(mutex);
    if (!active) {
        print_status_disabled = false;
        print_status_brightness = 100;
        return;
    }
    print_status_disabled = !val;
    print_status_brightness = val ? 100 : 0;
    old_state = StateAnimation::_last;
}

uint8_t StatusLedsHandler::get_print_status_brightness() {
    std::lock_guard lock(mutex);
    return print_status_brightness;
}

void StatusLedsHandler::set_print_status_brightness(uint8_t val) {
    if (val > 100) {
        val = 100;
    }
    std::lock_guard lock(mutex);
    print_status_brightness = val;
    print_status_disabled = print_status_brightness == 0;
    old_state = StateAnimation::_last;
}

void StatusLedsHandler::set_idle_light_state(LightState state) {
    std::lock_guard lock(mutex);
    idle_light_state = state;
}

void StatusLedsHandler::acknowledge_finished() {
    std::lock_guard lock(mutex);
    finished_acknowledged = true;
    finished_hold_until_ms = 0;
}

#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
void StatusLedsHandler::acknowledge_aborted() {
    std::lock_guard lock(mutex);
    aborted_acknowledged = true;
}
#endif

void StatusLedsHandler::update() {
    std::lock_guard lock(mutex);

    const bool print_active = print_active_for_status_override();
    const auto printer_state = marlin_vars().print_state.get();
    const bool filtering = post_filter_active();
    const bool finished = printer_state == State::Finished;
    const uint32_t now_ms = ticks_ms();
    const auto start_finished_hold = [&] {
        const uint32_t hold_s = config_store().status_led_finished_hold_s.get();
        finished_hold_until_ms = hold_s > 0 ? now_ms + hold_s * 1000 : 0;
    };

    if (print_active) {
        finished_hold_until_ms = 0;
        finished_acknowledged = false;
    } else if (!finished_acknowledged && ((filtering_prev && !filtering) || (finished && !finished_prev && !filtering))) {
        start_finished_hold();
    }
    filtering_prev = filtering;
    finished_prev = finished;
    const bool timed_finished_hold_active = finished_hold_until_ms != 0 && ticks_diff(now_ms, finished_hold_until_ms) < 0;

    if (!print_active) {
        print_status_disabled = false;
        print_status_brightness = 100;
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
    } else {
        aborted_acknowledged = false;
#endif
    }

    StateAnimation state;
    if (!active) {
        state = StateAnimation::Idle; // assuming LEDs are off in Idle
    } else if ((print_status_disabled || print_status_brightness == 0) && print_active) {
        state = StateAnimation::Idle;
    } else if (is_error_state) {
        state = StateAnimation::Error;
#if PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL()
    } else if (printer_state == State::Aborted && !aborted_acknowledged) {
        state = StateAnimation::Aborting;
#endif
    } else if (finished_acknowledged && (filtering || finished)) {
        state = StateAnimation::Idle;
    } else if (filtering && !finished_acknowledged) {
        state = StateAnimation::Filtering;
    } else if (timed_finished_hold_active && !finished_acknowledged) {
        state = StateAnimation::Finishing;
    } else {
        state = marlin_to_anim_state();
    }

    if (idle_light_state == LightState::deep_idle && state != StateAnimation::Filtering && state != StateAnimation::Finishing && state != StateAnimation::Aborting) {
        state = StateAnimation::Idle;
    }

    if (state == StateAnimation::Printing) {
        current_light_state = LightState::printing;
    } else if (state == StateAnimation::Idle || (state == StateAnimation::Finishing && finished && !timed_finished_hold_active)) {
        current_light_state = idle_light_state;
    } else {
        current_light_state = LightState::active;
    }

    if (state == StateAnimation::Printing) {
        finished_acknowledged = false;
    }

    auto animation = animations[state];
    animation.color = configured_color(state, animation.color);
    color = animation.color;
    if (state != old_state || color != old_color) {
        old_state = state;
        old_color = color;
        controller_instance().set(animation);
    }

    controller_instance().update();
}

std::span<const ColorRGBW, 3> StatusLedsHandler::led_data() {
    std::lock_guard lock(mutex);
    if (finished_acknowledged && post_filter_active()) {
        adjusted_data.fill({});
        return adjusted_data;
    }

    const auto data = controller_instance().data();
    uint8_t brightness = packed_brightness(brightness_by_state, current_light_state);
    if (print_active_for_status_override()) {
        brightness = static_cast<uint8_t>(static_cast<uint16_t>(brightness) * print_status_brightness / 100);
    }
    if (brightness >= 100) {
        return data;
    }

    for (size_t i = 0; i < adjusted_data.size(); ++i) {
        adjusted_data[i] = ColorRGBW(
            static_cast<uint8_t>(static_cast<uint16_t>(data[i].r) * brightness / 100),
            static_cast<uint8_t>(static_cast<uint16_t>(data[i].g) * brightness / 100),
            static_cast<uint8_t>(static_cast<uint16_t>(data[i].b) * brightness / 100),
            static_cast<uint8_t>(static_cast<uint16_t>(data[i].w) * brightness / 100));
    }
    return adjusted_data;
}

} // namespace leds

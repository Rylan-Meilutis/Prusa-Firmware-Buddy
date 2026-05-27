#include <leds/external_light_bar.hpp>

#include <config_store/store_instance.hpp>
#include <leds/side_strip_handler.hpp>
#include <logging/log.hpp>
#include <marlin_vars.hpp>
#include <timing.h>
#include "client_response.hpp"
#include "hwio_pindef.h"
#include <option/has_manual_belt_tuning.h>

LOG_COMPONENT_REF(Buddy);

namespace leds::external_light_bar {

static constexpr uint8_t low_side_pins_mask = 0x0F;
static constexpr uint8_t logic_pins_mask = 0xF0;
static constexpr uint8_t all_output_pins_mask = low_side_pins_mask | logic_pins_mask;
static constexpr uint32_t off_debounce_ms = 750;
static bool diagnostic_forced = false;
static bool diagnostic_on = false;

struct AppliedState {
    bool valid = false;
    bool on = false;
    uint8_t mask = 0;
    uint8_t active_high = 0;
};

static AppliedState applied_state;

static uint8_t pin_mask(uint8_t pin) {
    return static_cast<uint8_t>(0x1 << pin);
}

static bool belt_tuning_active() {
#if HAS_MANUAL_BELT_TUNING()
    bool active = false;
    marlin_vars().peek_fsm_states([&](const auto &states) {
        active = states.is_active(ClientFSM::ManualBeltTuning);
    });
    return active;
#else
    return false;
#endif
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

bool pin_supports_output(uint8_t pin) {
    return pin < buddy::hw::TCA6408A::pin_count && (all_output_pins_mask & pin_mask(pin));
}

bool pin_supports_active_high(uint8_t pin) {
    return pin < buddy::hw::TCA6408A::pin_count && (logic_pins_mask & pin_mask(pin));
}

uint8_t enabled_pin_mask() {
    return config_store().external_light_bar_enabled_pins.get();
}

uint8_t active_high_pin_mask() {
    return config_store().external_light_bar_active_high_pins.get();
}

bool state_enabled(leds::LightState state) {
    return config_store().external_light_bar_state_mask.get() & light_state_bit(state);
}

void set_state_enabled(leds::LightState state, bool enabled) {
    uint8_t mask = config_store().external_light_bar_state_mask.get();
    const uint8_t bit = light_state_bit(state);
    if (enabled) {
        mask |= bit;
    } else {
        mask &= ~bit;
    }
    config_store().external_light_bar_state_mask.set(mask);
}

OutputMode pin_mode(uint8_t pin) {
    if (!pin_supports_output(pin)) {
        return OutputMode::off;
    }

    const uint8_t mask = pin_mask(pin);
    if (!(enabled_pin_mask() & mask)) {
        return OutputMode::off;
    }
    if (active_high_pin_mask() & mask) {
        return OutputMode::active_high;
    }
    return OutputMode::pull_down;
}

void reset_persistent_state() {
    const uint8_t mask = protected_pin_mask();
    if (!mask) {
        return;
    }

    uint8_t output = config_store().io_expander_output_register.get();
    uint8_t config = config_store().io_expander_config_register.get();
    const uint8_t active_high = active_high_pin_mask() & mask;

    const uint8_t low_side = mask & low_side_pins_mask;
    const uint8_t logic_active_high = active_high;
    const uint8_t logic_active_sink = (mask & logic_pins_mask) & ~active_high;

    // Pins 0-3 use the low-side GPIO breakout path. The proven off state is
    // output mode with output latch low, matching M262 Pn B0 + M264 Pn B0.
    config &= ~low_side;
    output &= ~low_side;

    // Logic active-high pins are off when driven low.
    config &= ~logic_active_high;
    output &= ~logic_active_high;

    // Logic active-sink pins are off when floating.
    config |= logic_active_sink;
    output |= logic_active_sink;

    config_store().io_expander_output_register.set(output);
    config_store().io_expander_config_register.set(config);
    applied_state.valid = false;
}

static bool apply_mask(uint8_t mask, uint8_t active_high, bool on) {
    const uint8_t low_side = mask & low_side_pins_mask;
    const uint8_t logic_active_high = active_high & mask;
    const uint8_t logic_active_sink = (mask & logic_pins_mask) & ~active_high;

    if (on) {
        const uint8_t output_on = low_side | logic_active_high;
        return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, output_on, mask)
            && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask);
    }

    const uint8_t output_off = logic_active_sink;
    const uint8_t driven_off = low_side | logic_active_high;

    // Active-sink logic pins turn off by floating the pin. Float them before
    // setting their output latch high so the light never sees a driven pulse.
    return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0xFF, logic_active_sink)
        && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, output_off, mask)
        && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, driven_off);
}

static bool apply_pin(uint8_t pin, OutputMode mode, bool on) {
    const uint8_t mask = pin_mask(pin);
    const bool low_side_pin = low_side_pins_mask & mask;

    if (mode == OutputMode::active_high) {
        return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, on ? 0xFF : 0, mask)
            && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask);
    } else if (on) {
        if (low_side_pin) {
            return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask)
                && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, 0xFF, mask);
        }
        return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, 0, mask)
            && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask);
    } else {
        if (low_side_pin) {
            return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask)
                && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, 0, mask);
        }
        return buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0xFF, mask)
            && buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Output, 0xFF, mask);
    }
}

bool set_pin_mode(uint8_t pin, OutputMode mode) {
    if (!pin_supports_output(pin)) {
        return false;
    }
    if (mode == OutputMode::active_high && !pin_supports_active_high(pin)) {
        return false;
    }

    const uint8_t mask = pin_mask(pin);
    const auto previous_mode = pin_mode(pin);
    uint8_t enabled = enabled_pin_mask();
    uint8_t active_high = active_high_pin_mask() & logic_pins_mask;

    if (mode == OutputMode::off) {
        if (previous_mode != OutputMode::off) {
            apply_pin(pin, previous_mode, false);
        }
        enabled &= ~mask;
        active_high &= ~mask;
    } else {
        enabled |= mask;
        if (mode == OutputMode::active_high) {
            active_high |= mask;
        } else {
            active_high &= ~mask;
        }
    }

    config_store().external_light_bar_enabled_pins.set(enabled);
    config_store().external_light_bar_active_high_pins.set(active_high);
    reset_persistent_state();
    log_info(Buddy, "External light pin %u mode %u enabled 0x%02x active_high 0x%02x", pin, static_cast<unsigned>(mode), enabled, active_high);
    return true;
}

bool target_on([[maybe_unused]] bool chamber_light_on) {
    if (belt_tuning_active()) {
        return false;
    }

    const auto state = leds::SideStripHandler::instance().current_state();
    const bool target = state_enabled(light_state_for_strip_state(state));

    static bool off_pending = false;
    static uint32_t off_pending_since_ms = 0;

    if (target) {
        off_pending = false;
        return true;
    }

    if (!applied_state.valid || !applied_state.on) {
        off_pending = false;
        return false;
    }

    const uint32_t now = ticks_ms();
    if (!off_pending) {
        off_pending = true;
        off_pending_since_ms = now;
        return true;
    }

    if (now - off_pending_since_ms < off_debounce_ms) {
        return true;
    }

    off_pending = false;
    return false;
}

void apply(bool on) {
    if (diagnostic_forced) {
        on = diagnostic_on;
    }

    const uint8_t mask = protected_pin_mask();
    if (!mask) {
        applied_state.valid = false;
        return;
    }

    const uint8_t active_high = active_high_pin_mask() & mask;
    if (applied_state.valid
        && applied_state.on == on
        && applied_state.mask == mask
        && applied_state.active_high == active_high) {
        return;
    }

    const bool success = apply_mask(mask, active_high, on);

    static bool last_logged_on = false;
    static uint8_t last_logged_mask = 0;
    static uint8_t last_logged_active_high = 0;
    static bool last_logged_valid = false;
    if (success) {
        applied_state = { true, on, mask, active_high };
    }
    if (success && (!last_logged_valid || last_logged_on != on || last_logged_mask != mask || last_logged_active_high != active_high)) {
        uint8_t config_register = 0;
        uint8_t output_register = 0;
        uint8_t input_register = 0;
        buddy::hw::io_expander2.read_reg(buddy::hw::TCA6408A::Register::Config, config_register);
        buddy::hw::io_expander2.read_reg(buddy::hw::TCA6408A::Register::Output, output_register);
        buddy::hw::io_expander2.read_reg(buddy::hw::TCA6408A::Register::Input, input_register);
        log_info(Buddy, "External light %s mask 0x%02x active_high 0x%02x cfg 0x%02x out 0x%02x in 0x%02x", on ? "on" : "off", mask, active_high, config_register, output_register, input_register);
        last_logged_on = on;
        last_logged_mask = mask;
        last_logged_active_high = active_high;
        last_logged_valid = true;
    } else if (!success) {
        log_error(Buddy, "External light apply failed on %u mask 0x%02x", on, mask);
    }
}

void set_diagnostic_override(bool enabled, bool on) {
    diagnostic_forced = enabled;
    diagnostic_on = on;
    if (enabled) {
        apply(on);
    }
}

bool diagnostic_override_active() {
    return diagnostic_forced;
}

bool diagnostic_override_on() {
    return diagnostic_on;
}

bool protects_pin(uint8_t pin) {
    return pin < buddy::hw::TCA6408A::pin_count && (protected_pin_mask() & pin_mask(pin));
}

uint8_t protected_pin_mask() {
    return enabled_pin_mask() & all_output_pins_mask;
}

uint8_t protect_register_value(uint8_t requested_value, uint8_t current_value) {
    const uint8_t mask = protected_pin_mask();
    return (requested_value & ~mask) | (current_value & mask);
}

}

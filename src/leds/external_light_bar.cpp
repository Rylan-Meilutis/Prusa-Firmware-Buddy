#include <leds/external_light_bar.hpp>

#include <config_store/store_instance.hpp>
#include <logging/log.hpp>
#include "hwio_pindef.h"

LOG_COMPONENT_REF(Buddy);

namespace leds::external_light_bar {

static constexpr uint8_t low_side_pins_mask = 0x0F;
static constexpr uint8_t logic_pins_mask = 0xF0;
static constexpr uint8_t all_output_pins_mask = low_side_pins_mask | logic_pins_mask;
static bool diagnostic_forced = false;
static bool diagnostic_on = false;

static uint8_t pin_mask(uint8_t pin) {
    return static_cast<uint8_t>(0x1 << pin);
}

static void reset_persistent_pin_state(uint8_t mask) {
    config_store().io_expander_output_register.set(config_store().io_expander_output_register.get() | mask);
    config_store().io_expander_config_register.set(config_store().io_expander_config_register.get() | mask);
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
    reset_persistent_pin_state(mask);
    log_info(Buddy, "External light pin %u mode %u enabled 0x%02x active_high 0x%02x", pin, static_cast<unsigned>(mode), enabled, active_high);
    return true;
}

void apply(bool on) {
    if (diagnostic_forced) {
        on = diagnostic_on;
    }

    const uint8_t mask = protected_pin_mask();
    if (!mask) {
        return;
    }

    bool success = true;
    for (uint8_t pin = 0; pin < buddy::hw::TCA6408A::pin_count; ++pin) {
        if (mask & pin_mask(pin)) {
            success = apply_pin(pin, pin_mode(pin), on) && success;
        }
    }

    static bool last_logged_on = false;
    static uint8_t last_logged_mask = 0;
    static uint8_t last_logged_active_high = 0;
    static bool last_logged_valid = false;
    const uint8_t active_high = active_high_pin_mask() & mask;
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

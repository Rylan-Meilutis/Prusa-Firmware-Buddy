#include <leds/external_light_bar.hpp>

#include <config_store/store_instance.hpp>
#include "hwio_pindef.h"

namespace leds::external_light_bar {

static constexpr uint8_t low_side_pins_mask = 0x0F;
static constexpr uint8_t logic_pins_mask = 0xF0;
static constexpr uint8_t all_output_pins_mask = low_side_pins_mask | logic_pins_mask;

static uint8_t pin_mask(uint8_t pin) {
    return static_cast<uint8_t>(0x1 << pin);
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

static uint8_t output_value_for_mode(OutputMode mode, bool on) {
    if (mode == OutputMode::active_high) {
        return on ? 0xFF : 0;
    }
    return on ? 0 : 0xFF;
}

static void apply_pin(uint8_t pin, OutputMode mode, bool on) {
    const uint8_t mask = pin_mask(pin);
    buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask);
    buddy::hw::io_expander2.write(output_value_for_mode(mode, on), mask);
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
    return true;
}

static uint8_t output_value(bool on) {
    const uint8_t active_high = active_high_pin_mask() & enabled_pin_mask();
    const uint8_t pull_down = enabled_pin_mask() & ~active_high;
    return on ? active_high : pull_down;
}

void apply(bool on) {
    const uint8_t mask = protected_pin_mask();
    if (!mask) {
        return;
    }

    buddy::hw::io_expander2.update_register(buddy::hw::TCA6408A::Register::Config, 0, mask);
    buddy::hw::io_expander2.write(output_value(on), mask);
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

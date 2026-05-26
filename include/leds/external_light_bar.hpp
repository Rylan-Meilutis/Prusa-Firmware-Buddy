#pragma once

#include <stdint.h>
#include <leds/light_state.hpp>

namespace leds::external_light_bar {

enum class OutputMode : uint8_t {
    off = 0,
    pull_down = 1,
    active_high = 2,
};

bool pin_supports_output(uint8_t pin);
bool pin_supports_active_high(uint8_t pin);
OutputMode pin_mode(uint8_t pin);
uint8_t enabled_pin_mask();
uint8_t active_high_pin_mask();
bool state_enabled(leds::LightState state);
void set_state_enabled(leds::LightState state, bool enabled);

bool set_pin_mode(uint8_t pin, OutputMode mode);
bool target_on(bool chamber_light_on);
void apply(bool on);
void reset_persistent_state();
void set_diagnostic_override(bool enabled, bool on);
bool diagnostic_override_active();
bool diagnostic_override_on();

bool protects_pin(uint8_t pin);
uint8_t protected_pin_mask();
uint8_t protect_register_value(uint8_t requested_value, uint8_t current_value);

}

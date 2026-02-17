/// @file
#include "dwarf_hotend.hpp"

#include <module/prusa/toolchanger.h>
#include <common/aggregate_arity.hpp>

void DwarfHotend::set_nozzle_target_temp(TargetTemperature set) {
    BaseHotend::set_nozzle_target_temp(set);

    // !!! Do NOT use the set variable, the parent function can crop it
    prusa_toolchanger.getTool(tool_).set_hotend_target_temp(nozzle_target_temp());
}

void DwarfHotend::set_nozzle_pid_config(const HotendPIDConfig &set) {
    BaseHotend::set_nozzle_pid_config(set);
    buddy::puppies::dwarfs[tool_].set_pid(set.Kp, set.Ki, set.Kd);
    static_assert(aggregate_arity<HotendPIDConfig>() == 3);
}

void DwarfHotend::manage() {
    const bool is_current_tool = stdext::holds_value(PhysicalToolIndex::currently_selected(), tool_);

    auto &tool = prusa_toolchanger.getTool(tool_);
    nozzle_temp_ = tool.get_hotend_temp();
    nozzle_heater_pwm_ = static_cast<uint8_t>(tool.get_heater_pwm());

    // fan is regulted on dwarf - just update marlin's PWM value
    if (is_current_tool) {
        thermalManager.set_fan_speed(HEATBREAK_FAN_ID, tool.get_heatbreak_fan_pwr());
    }

    // !!! MUST be called after temps are set properly
    BaseHotend::manage();
}

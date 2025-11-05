///@file
#include <puppies/ac_controller.hpp>

#include <puppies/PuppyBootstrap.hpp>

using Lock = std::unique_lock<freertos::Mutex>;
using xbuddy_extension::NodeState;

namespace buddy::puppies {

AcController::AcController(PuppyModbus &bus, uint8_t modbus_address)
    : ModbusDevice(bus, modbus_address) {}

std::optional<float> AcController::get_mcu_temp() const {
    Lock lock(mutex);
    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<float>(status.value.mcu_temp) / 10.0f;
}

std::optional<float> AcController::get_bed_temp() const {
    Lock lock(mutex);
    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<float>(status.value.bed_temp) / 10.0f;
}

std::optional<float> AcController::get_bed_voltage() const {
    Lock lock(mutex);
    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<float>(status.value.bed_voltage) / 10.0f;
}

std::optional<std::array<uint16_t, 2>> AcController::get_bed_fan_rpm() const {
    Lock lock(mutex);

    if (!all_valid()) {
        return std::nullopt;
    }

    return status.value.bed_fan_rpm;
}

std::optional<uint16_t> AcController::get_psu_fan_rpm() const {
    Lock lock(mutex);

    if (!all_valid()) {
        return std::nullopt;
    }

    return status.value.psu_fan_rpm;
}

std::optional<uint8_t> AcController::get_bed_fan_pwm() const {
    Lock lock(mutex);

    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<uint8_t>(config.value.bed_fan_pwm);
}

std::optional<uint8_t> AcController::get_psu_fan_pwm() const {
    Lock lock(mutex);

    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<uint8_t>(config.value.psu_fan_pwm);
}

std::optional<ac_controller::Faults> AcController::get_faults() const {
    Lock lock(mutex);
    if (!all_valid()) {
        return std::nullopt;
    }

    uint32_t faults = static_cast<uint32_t>(status.value.faults_lo) | (static_cast<uint32_t>(status.value.faults_hi) << 16);
    return ac_controller::Faults { faults };
}

NodeState AcController::get_node_state() const {
    Lock lock(mutex);

    // This is intentionally not using all_valid()
    // because it would never return state other than ready.
    return valid ? static_cast<NodeState>(status.value.node_state) : NodeState::unknown;
}

void AcController::set_bed_target_temp(float target_temp) {
    Lock lock(mutex);

    uint16_t temp = static_cast<uint16_t>(target_temp * 10.0f);
    if (temp != config.value.bed_target_temp) {
        config.value.bed_target_temp = temp;
        config.dirty = true;
    }
}

void AcController::set_bed_fan_pwm(uint8_t pwm) {
    Lock lock(mutex);
    if (config.value.bed_fan_pwm != pwm) {
        config.value.bed_fan_pwm = pwm;
        config.dirty = true;
    }
}

void AcController::set_psu_fan_pwm(uint8_t pwm) {
    Lock lock(mutex);
    if (config.value.psu_fan_pwm != pwm) {
        config.value.psu_fan_pwm = pwm;
        config.dirty = true;
    }
}

void AcController::set_rgbw_led(std::array<uint8_t, 4> rgbw) {
    Lock lock(mutex);

    if (config.value.led_r != rgbw[0]) {
        config.value.led_r = rgbw[0];
        config.dirty = true;
    }
    if (config.value.led_g != rgbw[1]) {
        config.value.led_g = rgbw[1];
        config.dirty = true;
    }
    if (config.value.led_b != rgbw[2]) {
        config.value.led_b = rgbw[2];
        config.dirty = true;
    }
    if (config.value.led_w != rgbw[3]) {
        config.value.led_w = rgbw[3];
        config.dirty = true;
    }
}

CommunicationStatus AcController::refresh_input(uint32_t max_age) {
    // Already locked by caller.

    const auto result = bus.read(unit, status, max_age);

    switch (result) {
    case CommunicationStatus::OK:
        valid = true;
        break;
    case CommunicationStatus::ERROR:
        valid = false;
        break;
    default:
        // SKIPPED doesn't change the validity.
        break;
    }

    return result;
}

CommunicationStatus AcController::refresh_holding() {
    // Already locked by caller

    return bus.write(unit, config);
}

CommunicationStatus AcController::refresh() {
    Lock lock(mutex);

    const auto input = refresh_input(250);
    const auto holding = refresh_holding();

    if (input == CommunicationStatus::ERROR || holding == CommunicationStatus::ERROR) {
        return CommunicationStatus::ERROR;
    } else if (input == CommunicationStatus::SKIPPED && holding == CommunicationStatus::SKIPPED) {
        return CommunicationStatus::SKIPPED;
    } else {
        return CommunicationStatus::OK;
    }
}

CommunicationStatus AcController::initial_scan() {
    Lock lock(mutex);

    const auto input = refresh_input(0);
    config.dirty = true;
    return input;
}

bool AcController::all_valid() const {
    return valid && static_cast<NodeState>(status.value.node_state) == NodeState::ready;
}

AcController ac_controller(puppyModbus, 0x1a + 8);

} // namespace buddy::puppies

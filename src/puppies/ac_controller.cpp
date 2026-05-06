///@file
#include <puppies/ac_controller.hpp>

#include <modbus/server_address.hpp>
#include <utils/led_color.hpp>

using Lock = std::unique_lock<freertos::Mutex>;
using xbuddy_extension::NodeState;

static constexpr uint8_t unit = std::to_underlying(modbus::ServerAddress::ac_controller);

namespace buddy::puppies {

std::optional<float> AcController::get_mcu_temp() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<float>(cached_mcu_temp_dc.load()) / 10.0f;
}

std::optional<float> AcController::get_bed_temp() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<float>(cached_bed_temp_dc.load()) / 10.0f;
}

std::optional<float> AcController::get_bed_voltage() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return static_cast<float>(cached_bed_voltage_dv.load()) / 10.0f;
}

std::optional<std::array<uint16_t, 2>> AcController::get_bed_fan_rpm() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return std::array<uint16_t, 2> { cached_bed_fan_rpm[0].load(), cached_bed_fan_rpm[1].load() };
}

std::optional<uint16_t> AcController::get_psu_fan_rpm() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return cached_psu_fan_rpm.load();
}

std::optional<uint8_t> AcController::get_bed_fan_pwm() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return bed_fan_pwm_desired.load();
}

std::optional<uint8_t> AcController::get_psu_fan_pwm() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return psu_fan_pwm_desired.load();
}

std::optional<ac_controller::Faults> AcController::get_faults() const {
    if (!all_valid()) {
        return std::nullopt;
    }

    return ac_controller::Faults { cached_faults.load() };
}

NodeState AcController::get_node_state() const {
    // Intentionally not using all_valid() — that would never return a state
    // other than ready (chicken-and-egg with NodeState::ready).
    return valid.load() ? static_cast<NodeState>(cached_node_state.load()) : NodeState::unknown;
}

void AcController::set_bed_target_temp(float target_temp) {
    bed_target_temp_desired.store(static_cast<uint16_t>(target_temp * 10.0f));
}

void AcController::set_bed_fan_pwm(uint8_t pwm) {
    bed_fan_pwm_desired.store(pwm);
}

void AcController::set_psu_fan_pwm(uint8_t pwm) {
    psu_fan_pwm_desired.store(pwm);
}

void AcController::turn_off_bed_leds() {
    Lock lock(mutex);
    if (led_config.value.animation_type != 0) {
        led_config.value.animation_type = 0; // off
        led_config.dirty = true;
    }
}

void AcController::set_rgbw_led(std::array<uint8_t, 4> rgbw) {
    Lock lock(mutex);
    if (led_config.value.animation_type != 1) {
        led_config.value.animation_type = 1; // static_color
        led_config.dirty = true;
    }
    if (led_config.value.led_r != rgbw[0]) {
        led_config.value.led_r = rgbw[0];
        led_config.dirty = true;
    }
    if (led_config.value.led_g != rgbw[1]) {
        led_config.value.led_g = rgbw[1];
        led_config.dirty = true;
    }
    if (led_config.value.led_b != rgbw[2]) {
        led_config.value.led_b = rgbw[2];
        led_config.dirty = true;
    }
    if (led_config.value.led_w != rgbw[3]) {
        led_config.value.led_w = rgbw[3];
        led_config.dirty = true;
    }
}

void AcController::set_progress_percent(uint8_t percent) {
    constexpr auto progress_color = leds::ColorRGBW { 0, 150, 255, 0 };

    Lock lock(mutex);
    if (led_config.value.animation_type != 2) {
        led_config.value.animation_type = 2; // progress
        led_config.dirty = true;
    }
    if (led_config.value.progress_percent != percent) {
        led_config.value.progress_percent = percent;
        led_config.dirty = true;
    }
    if (led_config.value.led_r != progress_color.r || led_config.value.led_g != progress_color.g || led_config.value.led_b != progress_color.b || led_config.value.led_w != progress_color.w) {
        // Set to blue for progress
        led_config.value.led_r = progress_color.r;
        led_config.value.led_g = progress_color.g;
        led_config.value.led_b = progress_color.b;
        led_config.value.led_w = progress_color.w;
        led_config.dirty = true;
    }
}

CommunicationStatus AcController::refresh_input(PuppyModbus &bus, uint32_t max_age) {
    // Already locked by caller.

    const auto result = bus.read(unit, status, max_age);

    switch (result) {
    case CommunicationStatus::OK:
        cached_mcu_temp_dc.store(static_cast<int16_t>(status.value.mcu_temp));
        cached_bed_temp_dc.store(static_cast<int16_t>(status.value.bed_temp));
        cached_bed_voltage_dv.store(status.value.bed_voltage);
        cached_bed_fan_rpm[0].store(status.value.bed_fan_rpm[0]);
        cached_bed_fan_rpm[1].store(status.value.bed_fan_rpm[1]);
        cached_psu_fan_rpm.store(status.value.psu_fan_rpm);
        cached_faults.store(static_cast<uint32_t>(status.value.faults_lo) | (static_cast<uint32_t>(status.value.faults_hi) << 16));
        cached_node_state.store(status.value.node_state);
        // Only after all cached_* are published, so readers never see valid=true
        // with stale cached values.
        valid.store(true);
        break;
    case CommunicationStatus::ERROR:
        valid.store(false);
        break;
    default:
        // SKIPPED doesn't change the validity.
        break;
    }

    return result;
}

CommunicationStatus AcController::refresh(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto write = [&](uint16_t &dst, const uint16_t val) {
        if (val != dst) {
            dst = val;
            config.dirty = true;
        }
    };
    write(config.value.bed_target_temp, bed_target_temp_desired.load());
    write(config.value.bed_fan_pwm, static_cast<uint16_t>(bed_fan_pwm_desired.load()));
    write(config.value.psu_fan_pwm, static_cast<uint16_t>(psu_fan_pwm_desired.load()));

    const auto input = refresh_input(bus, 250);
    const auto holding_config = bus.write(unit, config);
    const auto holding_led_config = bus.write(unit, led_config);

    if (input == CommunicationStatus::ERROR || holding_config == CommunicationStatus::ERROR || holding_led_config == CommunicationStatus::ERROR) {
        return CommunicationStatus::ERROR;
    } else if (input == CommunicationStatus::SKIPPED && holding_config == CommunicationStatus::SKIPPED && holding_led_config == CommunicationStatus::SKIPPED) {
        return CommunicationStatus::SKIPPED;
    } else {
        return CommunicationStatus::OK;
    }
}

CommunicationStatus AcController::initial_scan(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto input = refresh_input(bus, 0);
    config.dirty = true;
    return input;
}

bool AcController::all_valid() const {
    return valid.load() && static_cast<NodeState>(cached_node_state.load()) == NodeState::ready;
}

AcController ac_controller;

} // namespace buddy::puppies

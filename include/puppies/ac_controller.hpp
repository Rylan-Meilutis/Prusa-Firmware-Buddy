///@file
#pragma once

#include "PuppyModbus.hpp"
#include <ac_controller/modbus.hpp>
#include <ac_controller/faults.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <freertos/mutex.hpp>
#include <option/has_ac_controller.h>
#include <optional>
#include <xbuddy_extension/shared_enums.hpp>

static_assert(HAS_AC_CONTROLLER());

namespace buddy::puppies {

/// Represents virtual AC Controller modbus device on the motherboard.
/// This handles synchronization between tasks and caching the data.
class AcController final {
public:
    // These are called from whatever task that needs them.
    std::optional<float> get_mcu_temp() const;
    std::optional<float> get_bed_temp() const;
    std::optional<float> get_bed_voltage() const;
    std::optional<std::array<uint16_t, 2>> get_bed_fan_rpm() const;
    std::optional<uint16_t> get_psu_fan_rpm() const;
    std::optional<uint8_t> get_bed_fan_pwm() const;
    std::optional<uint8_t> get_psu_fan_pwm() const;
    std::optional<ac_controller::Faults> get_faults() const;
    xbuddy_extension::NodeState get_node_state() const;
    void set_bed_target_temp(float);
    void set_bed_fan_pwm(uint8_t);
    void set_psu_fan_pwm(uint8_t);
    void turn_off_bed_leds();
    void set_rgbw_led(std::array<uint8_t, 4> rgbw);
    void set_progress_percent(uint8_t percent);

    // These are called from the puppy task.
    CommunicationStatus refresh(PuppyModbus &);
    CommunicationStatus initial_scan(PuppyModbus &);

private:
    // Serializes the puppy task's modbus operations with the LED-handler task's
    // led_config writes; cross-task access to mirrored fields is lock-free via atomics.
    mutable freertos::Mutex mutex;

    // --- Cached read-side state, populated by refresh_input() ---

    /// Set to true only after all cached_* mirrors are published, false on error.
    /// SKIPPED leaves it unchanged. Read lock-free from Marlin.
    std::atomic<bool> valid { false };

    // Mirror of status.value.mcu_temp (decidegree Celsius).
    std::atomic<int16_t> cached_mcu_temp_dc { 0 };

    // Mirror of status.value.bed_temp (decidegree Celsius).
    std::atomic<int16_t> cached_bed_temp_dc { 0 };

    // Mirror of status.value.bed_voltage (deci Volt).
    std::atomic<uint16_t> cached_bed_voltage_dv { 0 };

    // Mirrors of status.value.bed_fan_rpm[].
    std::array<std::atomic<uint16_t>, 2> cached_bed_fan_rpm {};

    // Mirror of status.value.psu_fan_rpm.
    std::atomic<uint16_t> cached_psu_fan_rpm { 0 };

    // Mirror of status.value.faults_lo | (faults_hi << 16).
    std::atomic<uint32_t> cached_faults { 0 };

    // Mirror of status.value.node_state.
    std::atomic<uint16_t> cached_node_state { 0 };

    // --- Desired write-side state, applied to config.value.* in refresh() ---

    std::atomic<uint16_t> bed_target_temp_desired { 0 };
    std::atomic<uint8_t> bed_fan_pwm_desired { 0 };
    std::atomic<uint8_t> psu_fan_pwm_desired { 0 };

    static_assert(std::atomic<bool>::is_always_lock_free);
    static_assert(std::atomic<int16_t>::is_always_lock_free);
    static_assert(std::atomic<uint16_t>::is_always_lock_free);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    static_assert(std::atomic<uint8_t>::is_always_lock_free);

    using Status = ac_controller::modbus::Status;
    using Config = ac_controller::modbus::Config;
    using LedConfig = ac_controller::modbus::LedConfig;
    ModbusInputRegisterBlock<Status::address, Status> status;
    ModbusHoldingRegisterBlock<Config::address, Config> config;
    ModbusHoldingRegisterBlock<LedConfig::address, LedConfig> led_config;

    CommunicationStatus refresh_input(PuppyModbus &, uint32_t max_age);

    bool all_valid() const;
};

extern AcController ac_controller;

} // namespace buddy::puppies

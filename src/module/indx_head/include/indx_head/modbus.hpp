//@file
#pragma once

#include <utils/storage/enum_bitset.hpp>
#include <indx_head/nozzle_presence.hpp>
#include <indx_head/leds.hpp>
#include <indx_head/errors.hpp>

#include <array>
#include <cstdint>
#include <type_traits>

// This file describes MODBUS registers for indx_head device.
// TODO: We currently mimic dwarf modbus layout, but this shouldn't be necessary.
// So feel free to move registers around as needed;

namespace indx_head::modbus {
/// These data are not changed during run of the device. they are loaded once on the first request if available (dev boards wont have otp).
struct StaticStatus {
    static constexpr uint16_t address = 0x8001;
    uint16_t hw_bom_id = 0;
    uint16_t hw_otp_timestamp_lo = 0;
    uint16_t hw_otp_timestamp_hi = 0;
    constexpr uint32_t hw_otp_timestamp() {
        return (static_cast<uint32_t>(hw_otp_timestamp_hi) << 16) | hw_otp_timestamp_lo;
    }
    std::array<uint16_t, 12> hw_raw_datamatrix {}; // 24B
};
static_assert(sizeof(StaticStatus) % 2 == 0);
static_assert(std::alignment_of_v<StaticStatus> == 2);

/// These data are updated regularly to reflect current status of the device.
/// Also these values are big subject to change, most of them won't be used by indx_head.
struct Status {
    static constexpr uint16_t address = 0x8060;
    errors::FaultStatusMask fault_status = indx_head::errors::FaultStatusMask::no_fault;
    uint16_t hotend_measured_temperature = 0;
    int16_t board_temperature = 0; // [int16_t degree C]
    int16_t mcu_temperature = 0; // [int16_t degree C]
    uint16_t print_fan_rpm = 0;
    uint16_t print_fan_pwm = 0;
    uint16_t print_fan_state = 0;
    uint16_t print_fan_is_rpm_ok = 0;
    uint16_t heaterbreak_fan_rpm = 0;
    uint16_t heaterbreak_fan_pwm = 0;
    uint16_t heaterbreak_fan_state = 0;
    uint16_t heaterbreak_fan_is_rpm_ok = 0;
    uint16_t system_24V_mV = 0; // [mV]
    uint16_t heater_current_mA = 0; // [mA]
    uint16_t time_sync_lo = 0;
    uint16_t time_sync_hi = 0;
    uint16_t nozzle_present = 0; // indx_head::NozzlePresence::unknown
    uint16_t nozzle_invalidation_ack = 0; // Echoed from xBuddy invalidate_nozzle_presence, returned after debouncer reset
    uint16_t nozzle_decay_x1000 = 0; // Last ringdown decay × 1000 (e.g. 96 = 0.096)
    // Diagnostics: bus-level error counters
    uint16_t diag_uart_errors = 0;
    uint16_t diag_i2c_errors = 0;
    uint16_t diag_spi_errors = 0;
    // Diagnostics: per-peripheral error counters
    uint16_t diag_i2c_thermo_errors = 0;
    uint16_t diag_i2c_led_errors = 0;
    uint16_t diag_spi_accel_errors = 0;
    uint16_t diag_spi_loadcell_errors = 0;

    static constexpr uint16_t time_sync_address() {
        return address + (offsetof(Status, time_sync_lo) / sizeof(Status::time_sync_lo));
    }
};
static_assert(sizeof(Status) % 2 == 0);
static_assert(std::alignment_of_v<Status> == 2);
static_assert(Status::address > StaticStatus::address + sizeof(StaticStatus) / 2);

/// These data are used to configure the device. Again some of the values might not be used, so feel free to change them (again only for initial Dwarf support).
struct Config {
    static constexpr uint16_t address = 0xE000;
    uint16_t nozzle_target_temperature = 0;
    struct {
        uint8_t value = 0;
        uint8_t padding = 0;
    } print_fan_pwm;
    indx_head::leds::LedConfig leds;
    uint16_t invalidate_nozzle_presence = 0;
    uint16_t loadcell_enabled = 0;
    uint16_t accelerometer_enabled = 0;
    static constexpr uint16_t loadcell_enabled_address() {
        return address + (offsetof(Config, loadcell_enabled) / sizeof(Config::loadcell_enabled));
    }
    static constexpr uint16_t accelerometer_enabled_address() {
        return address + (offsetof(Config, accelerometer_enabled) / sizeof(Config::accelerometer_enabled));
    }
};
static_assert(sizeof(Config) % 2 == 0);
static_assert(std::alignment_of_v<Config> == 2);
} // namespace indx_head::modbus

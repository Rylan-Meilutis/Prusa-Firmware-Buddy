#include "hal.hpp"
#include "critical_section.hpp"
#include "rtt.hpp"
#include <filters/debouncer.hpp>

#include <bsod/bsod.h>
#include <fpm/math.hpp>
#include <lp5817/lp5817.hpp>
#include <freertos/binary_semaphore.hpp>
#include <freertos/mutex.hpp>
#include <freertos/timing.hpp>
#include <raii/lock_guard.hpp>
#include <raii/scope_guard.hpp>
#include <stm32c0xx_hal.h>
#include <tpis/tpis.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <optional>
#include <ranges>
#include <utils/byte_utils.hpp>

namespace hal::peripherals {
extern I2C_HandleTypeDef hi2c1;
}

namespace hal::i2c {
namespace {

    freertos::Mutex i2c_mutex {};
    freertos::BinarySemaphore i2c_it_semaphore {};
    std::atomic<bool> i2c_error_flag { false };
    std::atomic<bool> waiting_for_i2c { false }; // Track if we're waiting for I2C completion

    // Timeout for I2C operations in milliseconds
    static constexpr uint32_t I2C_TIMEOUT_MS = 50;

    /// Attempts to recover I2C bus after an error
    void i2c_recover() {
        rtt::print("i2c: recover\n");
        // Reset I2C peripheral
        __HAL_I2C_DISABLE(&peripherals::hi2c1);
        // Small delay to allow bus to settle
        freertos::delay(1);
        __HAL_I2C_ENABLE(&peripherals::hi2c1);
        i2c_error_flag.store(false);
    }

    class I2cBus1 {
    public:
        [[nodiscard]] bool write_memory(::i2c::Address address, uint8_t offset, Bytes tx_buf) {
            return execute_i2c_operation(address, [&]() {
                uint8_t *data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(tx_buf.data()));
                return HAL_I2C_Mem_Write_DMA(&peripherals::hi2c1, (address << 1) | write_flag, offset, I2C_MEMADD_SIZE_8BIT, data, tx_buf.size());
            });
        }

        [[nodiscard]] bool read_memory(::i2c::Address address, uint8_t offset, WritableBytes rx_buf) {
            return execute_i2c_operation(address, [&]() {
                uint8_t *data = reinterpret_cast<uint8_t *>(rx_buf.data());
                return HAL_I2C_Mem_Read_DMA(&peripherals::hi2c1, (address << 1) | write_flag, offset, I2C_MEMADD_SIZE_8BIT, data, rx_buf.size());
            });
        }

        [[nodiscard]] bool raw_transmit(::i2c::Address address, Bytes tx_buf) {
            return execute_i2c_operation(address, [&]() {
                uint8_t *data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(tx_buf.data()));
                return HAL_I2C_Master_Transmit_DMA(&peripherals::hi2c1, (address << 1) | write_flag, data, tx_buf.size());
            });
        }

        [[nodiscard]] bool raw_receive(::i2c::Address address, WritableBytes rx_buf) {
            return execute_i2c_operation(address, [&]() {
                uint8_t *data = reinterpret_cast<uint8_t *>(rx_buf.data());
                return HAL_I2C_Master_Receive_DMA(&peripherals::hi2c1, (address << 1) | write_flag, data, rx_buf.size());
            });
        }

        void delay_us(uint32_t us) {
            uint32_t ms = us / 1'000;
            ms += (us - ms * 1'000) > 0;
            freertos::delay(ms + 1); // ensures at least 'ms' miliseconds wait
        }

    private:
        static constexpr uint8_t write_flag = 0;

        template <typename Op>
        [[nodiscard]] bool execute_i2c_operation(::i2c::Address address, Op op) {
            LockGuard lg { i2c_mutex };
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);
            const auto ret = op();
            if (ret != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, (address << 1) | write_flag);
                i2c_recover();
                return false;
            }
            if (i2c_error_flag.load()) {
                i2c_recover();
                return false;
            }
            return true;
        }
    };

    namespace thermometer {
        tpis::CalibrationParameters calibration {};
        static constexpr uint8_t address = 0b000'1100;

        bool do_general_call() {
            LockGuard lg { i2c_mutex };
            // F*CK you HAL I want to make this const(expr)
            static std::array<uint8_t, 2> data = { 0x04, 0x00 }; // General Call Reload
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);
            if (HAL_I2C_Master_Transmit_DMA(&peripherals::hi2c1, 0x00 /* General Call Address */, data.data(), data.size()) != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, 0x00);
                i2c_recover();
                return false;
            }
            return !i2c_error_flag.load();
        }

        enum class EepromSetting : uint8_t {
            disable = 0x00,
            enable = 0x80,
        };

        bool set_eeprom_reading(EepromSetting setting) {
            LockGuard lg { i2c_mutex };
            uint8_t data = static_cast<uint8_t>(setting);
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);
            if (HAL_I2C_Mem_Write_DMA(&peripherals::hi2c1, static_cast<uint16_t>(address << 1), 0x1f /* eeprom settings */, I2C_MEMADD_SIZE_8BIT, &data, sizeof(data)) != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1));
                i2c_recover();
                return false;
            }
            if (i2c_error_flag.load()) {
                i2c_recover();
                return false;
            }
            return true;
        }

        std::optional<tpis::SensorData> read_sensor_data() {
            LockGuard lg { i2c_mutex };
            std::array<std::byte, 4> raw_sensor_data {};

            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);

            HAL_StatusTypeDef status = HAL_I2C_Mem_Read_DMA(
                &peripherals::hi2c1,
                static_cast<uint16_t>(address << 1),
                0x1,
                I2C_MEMADD_SIZE_8BIT,
                reinterpret_cast<uint8_t *>(raw_sensor_data.data()),
                raw_sensor_data.size());

            if (status != HAL_OK) {
                // Failed to start I2C operation
                waiting_for_i2c.store(false);
                i2c_recover();
                return std::nullopt;
            }

            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                // Timeout waiting for I2C - recover and return invalid data
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1));
                i2c_recover();
                return std::nullopt;
            }

            if (i2c_error_flag.load()) {
                // I2C error occurred during transfer
                i2c_recover();
                return std::nullopt;
            }

            return tpis::decode_sensor_data(raw_sensor_data);
        }

        bool read_eeprom_calibration() {
            LockGuard lg { i2c_mutex };
            std::array<std::byte, 32> raw {};
            static constexpr uint8_t start_addr = 32;
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);

            if (HAL_I2C_Mem_Read_DMA(&peripherals::hi2c1, static_cast<uint16_t>(address << 1), start_addr, I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t *>(raw.data()), raw.size()) != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1));
                i2c_recover();
                return false;
            }
            if (i2c_error_flag.load()) {
                i2c_recover();
                return false;
            }

            auto cal = tpis::decode_calibration_parameters(raw);
            if (!cal.has_value()) {
                return false;
            }
            calibration = *cal;
            return true;
        }

        bool initialized = false;

        bool init() {
            if (!do_general_call()) {
                return false;
            }
            freertos::delay(2);

            set_eeprom_reading(EepromSetting::enable);
            bool ok = read_eeprom_calibration();
            set_eeprom_reading(EepromSetting::disable);

            initialized = ok;
            return ok;
        }
    } // namespace thermometer

    namespace leds {

        struct SetLEDDelayed {
            uint8_t r, g, b;
        };
        std::atomic<std::optional<SetLEDDelayed>> set_pwm_delayed { std::nullopt };
        std::atomic<std::optional<uint16_t>> set_fade_delayed { std::nullopt };

        using Controller = lp5817::LP5817<I2cBus1>;
        Controller controller;

        void init() {
            if (const auto res = controller.init(); !res.has_value()) {
                rtt::print("i2c: leds init failed");
            }
        }

        static constexpr uint8_t gamma_float(uint8_t in) {
            static constexpr float gamma = 2.2f; // Use 2.6 or 2.8 for richer colors
            static constexpr float rgb_max = 255.f;
            static constexpr float pwm_max = 255.f;
            return static_cast<uint8_t>(pwm_max * std::pow(float(in) / rgb_max, gamma));
        }

        static auto gamma_int_lut = []() consteval {
            std::array<uint8_t, 256> res {};
            const auto src = std::views::iota(0) | std::views::transform(gamma_float);
            std::ranges::copy_n(src.begin(), res.size(), res.begin());
            return res;
        }();

    } // namespace leds

} // namespace

void init_comm() {
    thermometer::init();
    leds::init();
}

CheckedTemperatureReading read_tpis_temperature() {
    static tpis::TemperatureReading last_reading {
        .object_temperature_celsius = tpis::fixed(25),
        .ambient_temperature_celsius = tpis::fixed(25),
    };

    // A jump > 50°C between consecutive readings is physically impossible
    static constexpr int32_t max_plausible_jump = 50; // °C, integer compare
    // Accept after 3 consecutive implausible readings
    static Debouncer<bool> temp_debouncer { false, 3 };

    if (!thermometer::initialized) {
        // Periodically retry thermometer init (every ~2s, called at 300Hz)
        static constexpr uint32_t REINIT_INTERVAL_MS = 2000;
        static uint32_t last_attempt_ms = 0;
        const uint32_t now = HAL_GetTick();
        if (now - last_attempt_ms >= REINIT_INTERVAL_MS) {
            last_attempt_ms = now;
            if (!thermometer::init()) {
                rtt::print("i2c: thermo init failed\n");
            }
        }
        return { .temps = last_reading, .valid = false };
    }
    // FIXME: Use scaled integers
    const auto measurement = thermometer::read_sensor_data();
    if (!measurement.has_value()) {
        rtt::print("i2c: thermo read failed\n");
        // Return last valid temperature on error
        return { .temps = last_reading, .valid = false };
    }

    const auto temps = tpis::calculate_temps(*measurement, thermometer::calibration);
    const int32_t diff = static_cast<int32_t>(temps.object_temperature_celsius) - static_cast<int32_t>(last_reading.object_temperature_celsius);
    const bool plausible = (diff > -max_plausible_jump) && (diff < max_plausible_jump);
    temp_debouncer.push(plausible);

    if (plausible) {
        last_reading = temps;
        return { .temps = temps, .valid = true };
    }

    if (temp_debouncer.is_stable() && !temp_debouncer.value()) {
        last_reading = temps;
        return { .temps = temps, .valid = true };
    }

    // Transient glitch — return last known good value
    return { .temps = last_reading, .valid = true };
}

void set_led_pwm(uint8_t r, uint8_t g, uint8_t b) {
    const auto pwm = leds::SetLEDDelayed { .r = leds::gamma_int_lut.at(r), .g = leds::gamma_int_lut.at(g), .b = leds::gamma_int_lut.at(b) };
    leds::set_pwm_delayed.store(pwm);
}

void set_led_fade(uint16_t interval_ms) {
    leds::set_fade_delayed.store(interval_ms);
}

void trigger_delayed_request() {
    const auto fade = leds::set_fade_delayed.exchange(std::nullopt);
    if (fade.has_value()) {
        const auto fade_time = leds::Controller::nearest_fade_time(*fade);
        const auto enable_fade = (*fade != 0);

        if (const auto res = leds::controller.set_fade_time(fade_time, enable_fade, enable_fade, enable_fade); !res.has_value()) {
            rtt::print("i2c: delayed leds fade failed");
        }

        return; // One request at a time
    }

    const auto pwm = leds::set_pwm_delayed.exchange(std::nullopt);
    if (pwm.has_value()) {
        if (const auto res = leds::controller.set_color(pwm->r, pwm->g, pwm->b); !res.has_value()) {
            rtt::print("i2c: delayed leds failed");
        }

        return; // One request at a time
    }
}

} // namespace hal::i2c

static void finish_i2c_transfer([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    debug_assert(hi2c == &hi2c1);
    if (hal::i2c::waiting_for_i2c.exchange(false)) {
        hal::i2c::i2c_it_semaphore.release_from_isr();
    }
}

extern "C" void HAL_I2C_MasterTxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    finish_i2c_transfer(hi2c);
}

extern "C" void HAL_I2C_MasterRxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    finish_i2c_transfer(hi2c);
}

extern "C" void HAL_I2C_MemTxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    finish_i2c_transfer(hi2c);
}

extern "C" void HAL_I2C_MemRxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    finish_i2c_transfer(hi2c);
}

extern "C" void HAL_I2C_ErrorCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    hal::i2c::i2c_error_flag.store(true);
    finish_i2c_transfer(hi2c);
}

#pragma once

#include <fpm/fixed.hpp>
#include <cstdint>
#include <optional>
#include <span>

namespace tpis {

constexpr size_t fraction_bits = 15;
using fixed = fpm::fixed<int32_t, int64_t, fraction_bits>;
constexpr size_t integral_bits = sizeof(fixed) * 8 - fraction_bits;

constexpr float emissivity = 0.48f;

struct SensorData {
    uint32_t tp_object = 0;
    uint16_t tp_ambient = 0;
};

struct CalibrationParameters {
    uint16_t ptat25 = 0;
    fixed m { 0 };
    uint32_t u0 = 0;
    uint32_t uout1 = 0;
    uint8_t t_obj1 = 0;
    fixed log2_k = fixed(0);
};

struct TemperatureReading {
    fixed object_temperature_celsius;
    fixed ambient_temperature_celsius;
};

SensorData decode_sensor_data(std::span<const std::byte, 4> raw_data);
std::optional<CalibrationParameters> decode_calibration_parameters(std::span<const std::byte, 32> raw_data);
TemperatureReading calculate_temps(SensorData measurement, const CalibrationParameters &calibration);

} // namespace tpis

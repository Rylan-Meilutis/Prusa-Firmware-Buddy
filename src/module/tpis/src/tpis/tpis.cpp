/// @file
#include <tpis/tpis.hpp>
#include <fpm/math.hpp>

namespace tpis {

SensorData decode_sensor_data(std::span<const std::byte, 4> raw_data) {
    uint32_t tp_object = (static_cast<uint32_t>(raw_data[0]) << 8 | static_cast<uint32_t>(raw_data[1])) << 1 | static_cast<uint32_t>(raw_data[2] >> 7);
    uint16_t tp_ambient = (static_cast<uint16_t>(raw_data[2] & std::byte { 0x7f }) << 8) | static_cast<uint16_t>(raw_data[3]);
    return SensorData { .tp_object = tp_object, .tp_ambient = tp_ambient };
}

bool validate_checksum(std::span<const std::byte, 32> data) {
    auto checksum = static_cast<uint16_t>(data[0]);
    const uint16_t expected_checksum = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);
    for (size_t i = 3; i < data.size(); ++i) {
        checksum += static_cast<uint8_t>(data[i]);
    }
    return checksum == expected_checksum;
}

std::optional<CalibrationParameters> decode_calibration_parameters(std::span<const std::byte, 32> raw_data) {
    const uint8_t protocol = static_cast<uint8_t>(raw_data[0]);
    if (protocol != 0x3) {
        return std::nullopt;
    }

    if (!validate_checksum(raw_data)) {
        return std::nullopt;
    }

    uint8_t lookup = static_cast<uint8_t>(raw_data[9]);
    if (lookup != 2) {
        return std::nullopt;
    }

    const uint16_t ptat25 = static_cast<uint16_t>(raw_data[10]) << 8 | static_cast<uint16_t>(raw_data[11]);
    const uint16_t raw_m_reg = static_cast<uint16_t>(raw_data[12]) << 8 | static_cast<uint16_t>(raw_data[13]);
    const fixed m = fixed(raw_m_reg) / 100;
    const auto raw_u0_reg = static_cast<uint16_t>(raw_data[14]) << 8 | static_cast<uint16_t>(raw_data[15]);
    const uint32_t u0 = raw_u0_reg + 32768;
    const auto raw_uout1_reg = static_cast<uint16_t>(raw_data[16]) << 8 | static_cast<uint16_t>(raw_data[17]);
    const uint32_t uout1 = raw_uout1_reg * 2;
    const uint8_t t_obj1 = static_cast<uint8_t>(raw_data[18]);

    const auto u_div = static_cast<int32_t>(uout1) - static_cast<int32_t>(u0);
    // NOTE: Expensive float op, but OK since it is ideally only done once at init (on failed comm it tries reinit every 2s)
    const auto f = [](float x) { return std::pow(x, f_exp_f); };
    const float k_f = static_cast<float>(u_div) / (f(t_obj1 + degC0asKf) - f(degC25asKf));
    const float log2_k_f = std::log2(k_f * tpis::emissivity);
    const fixed log2_k = fixed(log2_k_f);

    return CalibrationParameters {
        .ptat25 = ptat25,
        .m = m,
        .u0 = u0,
        .uout1 = uout1,
        .t_obj1 = t_obj1,
        .log2_k = log2_k
    };
}

TemperatureReading calculate_temps(SensorData measurement, const CalibrationParameters &calibration) {
    static_assert(fraction_bits >= 15);
    static_assert(integral_bits >= 17);
    const fixed t_ambient_k = degC25asK + fixed(static_cast<int32_t>(measurement.tp_ambient) - static_cast<int32_t>(calibration.ptat25)) / calibration.m;
    const fixed tp_relative = fixed(static_cast<int32_t>(measurement.tp_object) - static_cast<int32_t>(calibration.u0));
    constexpr fixed eps = fixed(1) / 1000;
    // https://en.wikipedia.org/wiki/LogSumExp#log-sum-exp_trick_for_log-domain_calculations
    const fixed lse_x = fpm::log2(fpm::abs(tp_relative) + eps) - calibration.log2_k;
    const fixed lse_y = f_exp * fpm::log2(t_ambient_k);
    const auto fixed_max = [](fixed x, fixed y) { return x > y ? x : y; };
    const auto checked_exp2 = [](fixed x) {
        return x < fixed(1 - fraction_bits) ? fixed(0) : fpm::exp2(x); // fpm::exp2 is broken on underflow
    };
    const fixed lse_exp = checked_exp2(-fpm::abs(lse_x - lse_y));
    const fixed lse_sum = tp_relative >= fixed(0) ? 1 + lse_exp : 1 - lse_exp;
    if (lse_sum <= fixed(0)) [[unlikely]] {
        abort();
    }
    const fixed lse = fixed_max(lse_x, lse_y) + fpm::log2(lse_sum);
    const fixed t_obj_k = fpm::exp2(F_exp * lse);
    const fixed object_temperature_celsius = t_obj_k - degC0asK;
    const fixed ambient_temperature_celsius = t_ambient_k - degC0asK;
    return TemperatureReading { object_temperature_celsius, ambient_temperature_celsius };
}

} // namespace tpis

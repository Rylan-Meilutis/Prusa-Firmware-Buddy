#include <catch2/catch_test_macros.hpp>
#include <tpis/tpis.hpp>

template <typename... Bytes>
constexpr auto make_bytes(Bytes... bytes) {
    return std::array<std::byte, sizeof...(Bytes)> { static_cast<std::byte>(bytes)... };
}

double calculate_k(const tpis::CalibrationParameters &calibration) {
    double u0 = static_cast<double>(calibration.u0);
    double uout1 = static_cast<double>(calibration.uout1);
    double t_obj1 = static_cast<double>(calibration.t_obj1);
    const auto f = [](double x) { return std::pow(x, 4.2); };
    double k_without_emissivity = (uout1 - u0) / (f(t_obj1 + 273.15) - f(25.0 + 273.15));
    double k = k_without_emissivity * tpis::emissivity;
    return k;
}

tpis::CalibrationParameters get_typical_cal_params() {
    auto cal = tpis::CalibrationParameters {
        .ptat25 = 13500,
        .m = tpis::fixed(172),
        .u0 = 64500,
        .uout1 = 67700,
        .t_obj1 = 100,
        .k_inv = 0.f,
    };
    const double k = calculate_k(cal);
    cal.k_inv = static_cast<float>(1 / k);
    return cal;
}

tpis::TemperatureReading calculate_temperature_reference(tpis::SensorData measurement, const tpis::CalibrationParameters &calibration) {
    double tp_object = static_cast<double>(measurement.tp_object);
    double tp_ambient = static_cast<double>(measurement.tp_ambient);
    double ptat25 = static_cast<double>(calibration.ptat25);
    double m = static_cast<double>(calibration.m);
    double u0 = static_cast<double>(calibration.u0);

    double t_ambient_k = (25.0 + 273.15) + (tp_ambient - ptat25) * (1 / m);
    const auto f = [](double x) { return std::pow(x, 4.2); };
    double k = calculate_k(calibration);
    const auto F = [](double x) { return std::pow(x, 1.0 / 4.2); };
    double t_obj_k = F((tp_object - u0) / k + f(t_ambient_k));
    return tpis::TemperatureReading {
        .object_temperature_celsius = static_cast<float>(t_obj_k - 273.15),
        .ambient_temperature_celsius = static_cast<float>(t_ambient_k - 273.15),
    };
}

uint32_t get_tp_object_for_temps(double t_obj_c, double t_ambient_c, const tpis::CalibrationParameters &calibration) {
    double u0 = static_cast<double>(calibration.u0);
    double k = calculate_k(calibration);
    const auto f = [](double x) { return std::pow(x, 4.2); };
    uint32_t tp_object = static_cast<uint32_t>(k * (f(t_obj_c + 273.15) - f(t_ambient_c + 273.15)) + u0);
    return tp_object;
}

uint16_t get_tp_ambient_for_temps(double t_obj_c, double t_ambient_c, const tpis::CalibrationParameters &calibration) {
    double ptat25 = static_cast<double>(calibration.ptat25);
    double m = static_cast<double>(calibration.m);
    double t_ambient = t_ambient_c + 273.15;
    double tp_ambient = (t_ambient - (25.0 + 273.15)) * m + ptat25;
    return static_cast<uint16_t>(tp_ambient);
}

tpis::SensorData get_measurement_for_temps(double t_obj_c, double t_ambient_c, const tpis::CalibrationParameters &calibration) {
    return tpis::SensorData {
        .tp_object = get_tp_object_for_temps(t_obj_c, t_ambient_c, calibration),
        .tp_ambient = get_tp_ambient_for_temps(t_obj_c, t_ambient_c, calibration)
    };
}

TEST_CASE("calculating temerature", "[tpis]") {
    const auto calibration = get_typical_cal_params();
    for (double t_ambient_c = -30.0; t_ambient_c < 110.0; t_ambient_c += 5.0) {
        for (double t_obj_c = -30.0; t_obj_c < 600.0; t_obj_c += 5.0) {
            const auto measurement = get_measurement_for_temps(t_obj_c, t_ambient_c, calibration);
            const auto temps_ref = calculate_temperature_reference(measurement, calibration);
            // small error is expected since tp_object and tp_ambient are integers
            CHECK(std::abs(temps_ref.object_temperature_celsius - t_obj_c) < 0.2);
            CHECK(std::abs(temps_ref.ambient_temperature_celsius - t_ambient_c) < 0.2);
            const auto temps = tpis::calculate_temps(measurement, calibration);
            CHECK(std::abs(temps.object_temperature_celsius - temps_ref.object_temperature_celsius) < 0.02);
            CHECK(std::abs(temps.ambient_temperature_celsius - temps_ref.ambient_temperature_celsius) < 0.02);
        }
    }
}

TEST_CASE("decode sensor data", "[tpis]") {
    {
        const auto sensor_data = tpis::decode_sensor_data(make_bytes(0xFF, 0xFF, 0x80, 0x00));
        CHECK(sensor_data.tp_object == 0x1FFFF);
        CHECK(sensor_data.tp_ambient == 0);
    }

    {
        const auto sensor_data = tpis::decode_sensor_data(make_bytes(0x00, 0x00, 0x7F, 0xFF));
        CHECK(sensor_data.tp_object == 0);
        CHECK(sensor_data.tp_ambient == 0x7FFF);
    }

    {
        const auto sensor_data = tpis::decode_sensor_data(make_bytes(0xFF, 0xFF, 0xFF, 0xFF));
        CHECK(sensor_data.tp_object == 0x1FFFF);
        CHECK(sensor_data.tp_ambient == 0x7FFF);
    }
}

TEST_CASE("decode calibration parameters", "[tpis]") {
    const auto raw_data = make_bytes(
        0x03, // protocol (3)
        0x03, 0xF9, // checksum
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
        0x02, // lookup (2)
        0x34, 0xBC, // ptat25 (13500)
        0x43, 0x30, // M (17200)
        0x7B, 0xF4, // U0 (31732)
        0x84, 0x3A, // U_out1 (33850)
        0x64, // T_obj1 (100)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
        0x00 // slave address
    );
    const auto calibration = tpis::decode_calibration_parameters(raw_data);
    CHECK(calibration.has_value());
    CHECK(calibration->ptat25 == 13500);
    CHECK(calibration->m == tpis::fixed(172));
    CHECK(calibration->u0 == 64500);
    CHECK(calibration->uout1 == 67700);
    CHECK(calibration->t_obj1 == 100);
    const auto expected_k = 8.27345242232e-8;
    const auto expected_k_inv_with_emissivity = 1 / (expected_k * tpis::emissivity);
    CHECK(std::abs(calibration->k_inv - expected_k_inv_with_emissivity) / expected_k_inv_with_emissivity < 1e-4); // relative error < 0.01%
}

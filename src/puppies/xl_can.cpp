#include <puppies/xl_can.hpp>

#include <logging/log.hpp>
#include <modbus/server_address.hpp>
#include <mutex>
#include <utility>
#include <puppies/cyphal_bridge.hpp>

LOG_COMPONENT_REF(Puppies);

namespace buddy::puppies {

namespace {
    using Lock = std::unique_lock<freertos::Mutex>;

    constexpr uint8_t unit = std::to_underlying(modbus::ServerAddress::xl_can);
} // namespace

CommunicationStatus XlCan::read_status(PuppyModbus &bus) {
    // Caller holds `mutex`.
    const auto result = bus.read(unit, status, 0);
    switch (result) {
    case CommunicationStatus::OK:
        valid.store(true);
        break;
    case CommunicationStatus::ERROR:
        valid.store(false);
        break;
    case CommunicationStatus::SKIPPED:
        break;
    }
    return result;
}

CommunicationStatus XlCan::ping(PuppyModbus &bus) {
    Lock lock(mutex);
    return read_status(bus);
}

CommunicationStatus XlCan::initial_scan(PuppyModbus &bus) {
    return ping(bus);
}

CommunicationStatus XlCan::refresh(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto status_result = read_status(bus);
    if (status_result == CommunicationStatus::OK) {
        if (const bool fault = (status.value.fan_power_fault != 0); fault != last_fan_power_fault) {
            last_fan_power_fault = fault;
            if (fault) {
                log_warning(Puppies, "xl_can: fan power switch FAULT (overcurrent/overtemperature)");
            } else {
                log_info(Puppies, "xl_can: fan power switch fault cleared");
            }
        }
    }

    cyphal_bridge.refresh(bus, modbus::ServerAddress::xl_can);

    return aggregate_communication_status({
        status_result,
        refresh_holding(bus),
    });
}

void XlCan::set_otp(const OTP_v5 &v) {
    Lock lock(mutex);
    otp = v;
}

OTP_v5 XlCan::get_otp() const {
    Lock lock(mutex);
    return otp;
}

void XlCan::set_fan_pwm(uint8_t pwm) {
    fan_pwm_desired.store(pwm);
}

std::optional<uint16_t> XlCan::get_fan_rpm() const {
    Lock lock(mutex);
    if (!valid.load()) {
        return std::nullopt;
    }
    return status.value.fan_rpm[xbuddy_extension::modbus::XL_CAN_FAN_IDX];
}

CommunicationStatus XlCan::set_modular_bed_reset(PuppyModbus &bus, bool nreset) {
    Lock lock(mutex);
    mmu_nreset_desired.store(nreset);
    config.dirty = true;

    // Send immediately - needed for the modular bed boodstrap
    return refresh_holding(bus);
}

CommunicationStatus XlCan::refresh_holding(PuppyModbus &bus) {
    // Caller holds `mutex`.
    const auto write = [&](uint16_t &dst, const uint16_t val) {
        if (val != dst) {
            dst = val;
            config.dirty = true;
        }
    };
    write(config.value.mmu_nreset, static_cast<uint16_t>(mmu_nreset_desired.load() ? 1 : 0));
    write(config.value.fan_pwm[xbuddy_extension::modbus::XL_CAN_FAN_IDX], fan_pwm_desired.load());
    return bus.write(unit, config);
}

XlCan xl_can;

} // namespace buddy::puppies

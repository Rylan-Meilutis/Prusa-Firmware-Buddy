#include <puppies/xl_can.hpp>

#include <modbus/server_address.hpp>
#include <mutex>
#include <utility>
#include <puppies/cyphal_bridge.hpp>

namespace buddy::puppies {

namespace {
    using Lock = std::unique_lock<freertos::Mutex>;

    constexpr uint8_t unit = std::to_underlying(modbus::ServerAddress::xl_can);
} // namespace

CommunicationStatus XlCan::ping(PuppyModbus &bus) {
    Lock lock(mutex);
    return bus.read(unit, status, 0);
}

CommunicationStatus XlCan::initial_scan(PuppyModbus &bus) {
    return ping(bus);
}

CommunicationStatus XlCan::refresh(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto status_result = bus.read(unit, status, 0);

    cyphal_bridge.refresh(bus, modbus::ServerAddress::xl_can);

    return status_result;
}

void XlCan::set_otp(const OTP_v5 &v) {
    Lock lock(mutex);
    otp = v;
}

OTP_v5 XlCan::get_otp() const {
    Lock lock(mutex);
    return otp;
}

XlCan xl_can;

} // namespace buddy::puppies

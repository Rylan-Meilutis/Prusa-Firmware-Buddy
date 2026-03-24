#include "filament_sensor_xbuddy_extension.hpp"

#include <feature/xbuddy_extension/xbuddy_extension.hpp>

FSensorXBuddyExtension::FSensorXBuddyExtension(FilamentSensorID id, Source source)
    : IFSensor(id)
    , source_(source) {}

void FSensorXBuddyExtension::cycle() {
    state = interpret_state();
}

int32_t FSensorXBuddyExtension::GetFilteredValue() const {
    return static_cast<int32_t>(interpret_state());
}

FilamentSensorState FSensorXBuddyExtension::interpret_state() const {
    switch (buddy::xbuddy_extension().status()) {

    case buddy::XBuddyExtension::Status::disabled:
        return FilamentSensorState::Disabled;

    case buddy::XBuddyExtension::Status::not_connected:
        return FilamentSensorState::NotConnected;

    case buddy::XBuddyExtension::Status::ready:
        // Continue
        break;
    }

    std::optional<buddy::XBuddyExtension::FilamentSensorState> state;
    switch (source_) {

    case Source::gpio:
        state = buddy::xbuddy_extension().gpio_filament_sensor();
        break;

    case Source::ext:
        state = buddy::xbuddy_extension().ext_filament_sensor(id_.index);
        break;
    }

    switch (state.value_or(buddy::XBuddyExtension::FilamentSensorState::uninitialized)) {

    case buddy::XBuddyExtension::FilamentSensorState::disconnected:
        return FilamentSensorState::NotConnected;

    case buddy::XBuddyExtension::FilamentSensorState::uninitialized:
        return FilamentSensorState::NotInitialized;

    case buddy::XBuddyExtension::FilamentSensorState::has_filament:
        return FilamentSensorState::HasFilament;

    case buddy::XBuddyExtension::FilamentSensorState::no_filament:
        return FilamentSensorState::NoFilament;
    }

    return FilamentSensorState::NotInitialized;
}

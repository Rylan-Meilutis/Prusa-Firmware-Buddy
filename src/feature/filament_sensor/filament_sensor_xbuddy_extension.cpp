/// @file
#include "filament_sensor_xbuddy_extension.hpp"

#include <feature/xbuddy_extension/xbuddy_extension.hpp>
#include <option/has_side_fsensor_invertible.h>
#include <metric.h>

METRIC_DEF(metric_side, "side_fsensor", METRIC_VALUE_CUSTOM, 49, METRIC_DISABLED);

FSensorXBuddyExtension::FSensorXBuddyExtension(FilamentSensorID id, Source source)
    : FSensorXBuddyExtensionParent(id)
    , source_(source) {}

void FSensorXBuddyExtension::cycle() {
#if IS_XBE_FSENSOR_INVERTIBLE()
    raw_state_ = interpret_state();
    FSensorInvertible::cycle();
#else
    state = interpret_state();
#endif
}

void FSensorXBuddyExtension::record_state() {
    metric_record_custom(&metric_side, ",n=%u st=%ui", id_.index, static_cast<unsigned>(get_state()));
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

    std::optional<buddy::XBuddyExtension::FilamentSensorState> hw_state;
    switch (source_) {

    case Source::gpio:
        hw_state = buddy::xbuddy_extension().gpio_filament_sensor();
        break;

    case Source::ext:
        hw_state = buddy::xbuddy_extension().ext_filament_sensor(id_.index);
        break;
    }

    switch (hw_state.value_or(buddy::XBuddyExtension::FilamentSensorState::uninitialized)) {

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

#pragma once

#include <option/has_indx.h>

#include <feature/filament_sensor/filament_sensor.hpp>

// INDX side fsensors can have the magnet inverted. This is not the case for the standard C1
#define IS_XBE_FSENSOR_INVERTIBLE() HAS_INDX()

#if IS_XBE_FSENSOR_INVERTIBLE()
    #include <feature/filament_sensor/filament_sensor_invertible.hpp>
using FSensorXBuddyExtensionParent = FSensorInvertible;
#else
using FSensorXBuddyExtensionParent = IFSensor;
#endif

class FSensorXBuddyExtension : public FSensorXBuddyExtensionParent {

public:
    enum class Source {
        /// Single fsensor on the GPIO connector
        gpio,

        /// OneWire-based array of sensors on the EXT connector
        ext,
    };

    FSensorXBuddyExtension(FilamentSensorID id, Source source);

protected:
    virtual void cycle() override;

private:
    FilamentSensorState interpret_state() const;

    const Source source_;
};

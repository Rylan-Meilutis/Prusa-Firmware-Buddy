#pragma once

#include <feature/filament_sensor/filament_sensor.hpp>

class FSensorXBuddyExtension : public IFSensor {

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
    virtual int32_t GetFilteredValue() const override;

private:
    FilamentSensorState interpret_state() const;

    const Source source_;
};

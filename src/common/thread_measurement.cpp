/// @file
#include <common/thread_measurement.h>

#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <common/metric.h>
#include <feature/tmc_util.h>
#include <freertos/timing.hpp>

void StartMeasurementTask() {
    for (;;) {
        freertos::delay(50);
    }
}

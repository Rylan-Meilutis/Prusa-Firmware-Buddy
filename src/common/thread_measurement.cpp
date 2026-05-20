/// @file
#include <common/thread_measurement.h>

#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <common/metric.h>
#include <feature/tmc_util.h>
#include <freertos/timing.hpp>

void StartMeasurementTask() {
    FSensors_instance().task_init();
    for (;;) {
        FSensors_instance().task_cycle();
        freertos::delay(50);
    }
}

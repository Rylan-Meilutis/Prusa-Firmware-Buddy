#include "hal.hpp"
#include "nfc.hpp"

#include <FreeRTOS.h>
#include <task.h>
#include <freertos/timing.hpp>

// This magical incantation is required for fw_descriptor integration in cmake to work.
[[maybe_unused]] __attribute__((section(".fw_descriptor"), used)) const std::byte fw_descriptor[48] {};

constexpr const size_t main_task_stack_size = 200;
alignas(32) StackType_t main_task_stack[main_task_stack_size];
StaticTask_t main_task_control_block;

static void main_task_func(void *) {
    nfc::init();

    while (true) {
        hal::set_status_led(true);
        freertos::delay(1000);
        hal::set_status_led(false);
        freertos::delay(1000);
    }
}

extern "C" int main() {
    hal::init();

    xTaskCreateStatic(
        main_task_func,
        "main_task",
        main_task_stack_size,
        NULL,
        tskIDLE_PRIORITY + 1,
        main_task_stack,
        &main_task_control_block);

    // Start FreeRTOS scheduler and we are done.
    vTaskStartScheduler();
}

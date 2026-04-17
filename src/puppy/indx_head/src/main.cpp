#pragma GCC poison printf
#include "hal.hpp"
#include "rtt.hpp"
#include "app.hpp"

#include <FreeRTOS.h>
#include <task.h>

#include <cstddef>

// This magical incantation is required for fw_descriptor integration in cmake to work.
[[maybe_unused]] __attribute__((section(".fw_descriptor"), used)) const std::byte fw_descriptor[48] {};

namespace {
// App task
constexpr const size_t app_task_stack_size = 2 * 512 / sizeof(StackType_t);
alignas(32) StackType_t app_task_stack[app_task_stack_size];
StaticTask_t app_task_control_block;
} // namespace

extern "C" int main() {
    hal::init();
    rtt::init();
    rtt::print("indx_head started\n");

    {
        [[maybe_unused]] TaskHandle_t app_task_handle = xTaskCreateStatic(
            [](void *) { app::run(); },
            "app_task",
            app_task_stack_size,
            nullptr,
            tskIDLE_PRIORITY + 1,
            app_task_stack,
            &app_task_control_block);
    }

    // Start FreeRTOS scheduler and we are done.
    vTaskStartScheduler();
}

extern "C" void vApplicationStackOverflowHook([[maybe_unused]] TaskHandle_t xTask, [[maybe_unused]] char *pcTaskName) {
    hal::panic(indx_head::errors::FaultStatusMask::stack_overflow);
}

extern "C"
    [[noreturn, gnu::format(__printf__, 1, 4)]] void
    _bsod([[maybe_unused]] const char *fmt, [[maybe_unused]] const char *file_name, [[maybe_unused]] int line_number, ...) {
    hal::panic(indx_head::errors::FaultStatusMask::assert_failed);
}

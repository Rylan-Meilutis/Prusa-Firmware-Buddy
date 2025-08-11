
#include <device/multi_watchdog.hpp>

#include <bsod.h>
#include <option/disable_watchdog.h>
#include <cmsis_compiler.h>
#include <logging/log.hpp>
#include <device/hal.h>

extern void on_watchdog_early_warning();
namespace device {

LOG_COMPONENT_DEF(WDG, logging::Severity::error);

MultiWatchdog *MultiWatchdog::list = nullptr;
std::function<void(void)> MultiWatchdog::refresh = nullptr;
uint32_t MultiWatchdog::last_refresh_time = 0;

MultiWatchdog::MultiWatchdog(const char *name) {
    this->name = name;
    // Add itself on top of list
    __disable_irq();
    next = list;
    list = this;
    __enable_irq();
}

MultiWatchdog::~MultiWatchdog() {
    // Removing watchdog is not allowed
    bsod("Watchdog deleted");
}

void MultiWatchdog::init([[maybe_unused]] std::function<void(void)> init_hw, [[maybe_unused]] std::function<void(void)> refresh_) {
#if !DISABLE_WATCHDOG()
    __disable_irq();
    if (refresh != nullptr) {
        __enable_irq();
        return;
    }

    init_hw();

    refresh = refresh_;
    __enable_irq();
#endif /*!DISABLE_WATCHDOG()*/
    last_refresh_time = HAL_GetTick();
}

void MultiWatchdog::kick(bool hardware) {
    mark = 0xff; // Mark itself
    if (hardware) {
        check_all(); // Check others
    }
}

void MultiWatchdog::early_check_sw(uint32_t limit) {
    if (HAL_GetTick() - last_refresh_time > limit) {
        log_unkicked();
        on_watchdog_early_warning();
    }
}

void MultiWatchdog::log_unkicked() {
    for (device::MultiWatchdog *item = list; item != nullptr; item = item->next) {
        if (item->mark == 0) {
            log_critical(WDG, "Watchdog '%s' not kicked", item->name);
        }
    }
}

void MultiWatchdog::check_all() {
    if (refresh == nullptr) {
        return;
    }

    /**
     * @note No __disable_irq is used here.
     * Watchdog doesn't mind that it may accidentally get reloaded twice.
     */

    // Check the entire list
    for (MultiWatchdog *item = list; item != nullptr; item = item->next) {
        if (item->mark == 0) // Someone didn't kick
        {
            return;
        }
    }

    // Clear the entire list
    for (MultiWatchdog *item = list; item != nullptr; item = item->next) {
        item->mark = 0;
    }

    // Reload hardware
    refresh();
    last_refresh_time = HAL_GetTick();
}

void MultiWatchdogExtended::extended_kick() {
    extender = 0;
    kick(false);
}

void MultiWatchdogExtended::tick(uint32_t limit) {
    auto extender_copy = extender.load();

    // Kick if we are under the limit
    if (extender_copy < limit) {
        kick(false);
    }

    // Increment
    /// @note Do not use compare_exchange_strong() as it is not supported on STM32G0.
    if (extender.exchange(extender_copy + 1) != extender_copy) {
        extender = 0; // It was kicked while we were adding 1
    }
}

} // namespace device

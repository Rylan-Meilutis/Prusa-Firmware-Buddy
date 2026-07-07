/// @file
#include "MItem_development.hpp"

#include <config_store/store_instance.hpp>
#include <common/timing.h>
#include <logging/log.hpp>
#include <window_msgbox.hpp>
#include <Marlin/src/core/serial.h>

#include <cinttypes>
#include <cstdio>

LOG_COMPONENT_REF(GUI);

MI_DRY_RUN::MI_DRY_RUN()
    : WI_ICON_SWITCH_OFF_ON_t {
        static_cast<bool>(marlin_debug_flags & MARLIN_DEBUG_DRYRUN),
        _("Dry run (no extrusion)"),
        nullptr,
        is_enabled_t::yes,
        is_hidden_t::dev,
    } {
}

void MI_DRY_RUN::OnChange(size_t) {
    // marlin_debug_flags should be accessed only from the marlin thread.
    // Ideally the M111 should be expanded for setting/resetting individual bits, but:
    // * this menu item is dev-only
    // * there's not much this can screw up
    // * this is actually safer, because the read and write is close together (when issuing M111 with all flags override, there's more change of a race condition)

    if (value()) {
        marlin_debug_flags |= MARLIN_DEBUG_DRYRUN;
    } else {
        marlin_debug_flags &= ~MARLIN_DEBUG_DRYRUN;
    }
}

MI_TRIGGER_BANK_MIGRATION::MI_TRIGGER_BANK_MIGRATION()
    : IWindowMenuItem {
        /// dev item intentionally not translated
        string_view_utf8::MakeCPUFLASH("Trigger Bank Migration"),
        nullptr,
        is_enabled_t::yes,
        is_hidden_t::dev,
    } {}

void MI_TRIGGER_BANK_MIGRATION::click(IWindowMenu &) {
    const uint32_t start_ms = ticks_ms();
    config_store().get_backend().trigger_bank_migration();
    const int32_t elapsed_ms = ticks_diff(ticks_ms(), start_ms);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Bank migration took %" PRId32 " ms", elapsed_ms);
    log_info(GUI, "%s", buffer);
    /// dev item intentionally not translated
    MsgBoxInfo(string_view_utf8::MakeRAM(buffer), Responses_Ok);
}

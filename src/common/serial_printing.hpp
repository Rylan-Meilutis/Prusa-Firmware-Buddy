
#pragma once
#include "marlin_server.hpp"
#include "timing.h"
#include "gcode/queue.h"
#include "module/planner.h"
#include <serial_printing_ui_mode.hpp>

/// Helper class to better support serial printing
class SerialPrinting {
public:
    /// Notifies print host about print being paused
    static void pause();

    /// Notifies print host about a firmware-side pause reason.
    static void paused(const char *reason);

    /// periodically called when in state "printing"
    static void print_loop();

    /// Notify host about print abort
    static void abort();

    /// Notify host about print resume
    static void resume();

    /// Notify host that firmware-side pause/error condition has cleared.
    static void resumed();

    /// Hook called when command is queued via serial line
    /// Used to detect activity of serial print
    /// Returns false when the command should not be queued.
    static bool serial_command_hook(const char *command);

    /// Serial hosts can report progress outside of the printed G-code stream
    /// (for example in M117 status text). Prefer that over queued M73 values.
    static bool host_progress_percent(uint8_t &percent, uint32_t now_ms);
    static void set_host_progress_percent(uint8_t percent);
    static bool host_time_to_end(uint32_t &seconds, uint32_t now_ms);
    static void set_host_time_to_end(uint32_t seconds);

    static SerialPrintingUiMode ui_mode();

private:
    static void host_action(const char *action, const char *reason = nullptr);

    /// Check if serial printing had timeouted
    /// ie no command was queued for a while
    static bool has_serial_timeouted();

    /// Timeout [ms], after which serial print will be considered as finished
    static uint32_t serial_printing_screen_timeout_ms();

    static void reset_host_progress();

    /// Last time of activity of serial print
    static uint32_t last_serial_indicator_ms;

    static uint8_t last_host_progress_percent;
    static uint32_t last_host_progress_ms;
    static uint32_t last_host_time_to_end_s;
    static uint32_t last_host_time_to_end_ms;
    static bool pending_start;
};

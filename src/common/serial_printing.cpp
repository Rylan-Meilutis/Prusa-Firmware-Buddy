#include <serial_printing.hpp>
#include <state/printer_state.hpp>
#include <option/developer_mode.h>
#include <config_store/store_instance.hpp>
#include <cstdlib>

uint32_t SerialPrinting::last_serial_indicator_ms = 0;

void SerialPrinting::print_loop() {
    if (has_serial_timeouted()) {
        marlin_server::serial_print_finalize();
    }
}

void SerialPrinting::abort() {
    marlin_server::enqueue_gcode("M118 A1 action:cancel");
}

void SerialPrinting::resume() {
    last_serial_indicator_ms = ticks_ms();
    GCodeQueue::pause_serial_commands = false;
    marlin_server::enqueue_gcode("M118 A1 action:resume");
}

void SerialPrinting::pause() {
    GCodeQueue::pause_serial_commands = false;
    marlin_server::enqueue_gcode("M118 A1 action:pause");
}

bool SerialPrinting::has_serial_timeouted() {
    auto curr_time { ticks_ms() };
    if (GCodeQueue::has_commands_queued() || planner.processing()) {
        // refresh timer if there are commands still processing, we want to timeout after command finishes, not after when its queued
        last_serial_indicator_ms = curr_time;
        return false;
    }

    if (last_serial_indicator_ms != 0 && last_serial_indicator_ms <= curr_time && curr_time <= last_serial_indicator_ms + serial_printing_screen_timeout) {
        return false;
    } else {
        return true;
    }
}

void remove_N_prefix(const char *&command) {
    while (*command == ' ') {
        ++command;
    }

    if (*command == 'N') {
        ++command;
        while (*command != ' ') {
            if (*command == '\0') {
                return;
            }
            ++command;
        }
        ++command;
    }
}

bool command_starts_with(const char *command, char letter, int code) {
    while (*command == ' ') {
        ++command;
    }

    if (*command == '\0') {
        return false;
    }

    if (*command != letter) {
        return false;
    }

    char *end = nullptr;
    const auto parsed_code = strtol(command + 1, &end, 10);
    return end != command + 1 && parsed_code == code && (*end == '\0' || *end == ' ' || *end == ';' || *end == '*');
}

bool find_param_int(const char *command, char param, int &value) {
    for (const char *p = command; *p != '\0' && *p != ';' && *p != '*'; ++p) {
        if (*p != param) {
            continue;
        }

        char *end = nullptr;
        const auto parsed = strtol(p + 1, &end, 10);
        if (end == p + 1) {
            continue;
        }

        value = parsed;
        return true;
    }

    return false;
}

bool m73_progress_is(const char *command, int target_progress) {
    if (!command_starts_with(command, 'M', 73)) {
        return false;
    }

    int progress = 0;
    return (find_param_int(command, 'P', progress) || find_param_int(command, 'Q', progress)) && progress == target_progress;
}

bool m73_finished(const char *command) {
    if (!m73_progress_is(command, 100)) {
        return false;
    }

    int remaining = 0;
    return (find_param_int(command, 'R', remaining) || find_param_int(command, 'S', remaining)) && remaining == 0;
}

bool print_start_gcode(const char *command) {
    return command_starts_with(command, 'M', 75) || m73_progress_is(command, 0);
}

bool print_end_gcode(const char *command) {
    return command_starts_with(command, 'M', 77) || m73_finished(command);
}

void SerialPrinting::serial_command_hook(const char *command) {
    // never enter serial printing in developer mode as it breaks live debugging
    if (option::developer_mode) {
        return;
    }

    if (!config_store().serial_print_screen_enabled.get()) {
        return;
    }

    remove_N_prefix(command);

    if (marlin_server::serial_print_active()) {
        last_serial_indicator_ms = ticks_ms();
        if (print_end_gcode(command)) {
            marlin_server::serial_print_finalize();
        }
        return;
    }

    // If marlin server is already printing, or is not able to start print, do not enter serial printing state.
    // The command will still be queued for execution regardless of this.
    if (!printer_state::remote_print_ready(true)) {
        return;
    }

    if (print_start_gcode(command)) {
        last_serial_indicator_ms = ticks_ms();
        marlin_server::serial_print_start();
    }
}

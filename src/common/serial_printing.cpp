#include <serial_printing.hpp>
#include <state/printer_state.hpp>
#include <option/developer_mode.h>
#include <config_store/store_instance.hpp>
#include "../Marlin/src/gcode/lcd/M73_PE.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

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

const char *command_arg(const char *command) {
    while (*command == ' ') {
        ++command;
    }

    if (*command == '\0') {
        return command;
    }

    ++command;
    while (*command >= '0' && *command <= '9') {
        ++command;
    }

    while (*command == ' ') {
        ++command;
    }

    return command;
}

bool ascii_iequals(char a, char b) {
    if (a >= 'A' && a <= 'Z') {
        a = a - 'A' + 'a';
    }
    if (b >= 'A' && b <= 'Z') {
        b = b - 'A' + 'a';
    }
    return a == b;
}

const char *find_case_insensitive(const char *haystack, const char *needle) {
    for (const char *h = haystack; *h; ++h) {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np && ascii_iequals(*hp, *np)) {
            ++hp;
            ++np;
        }
        if (*np == '\0') {
            return h;
        }
    }

    return nullptr;
}

bool parse_percent(const char *message, uint8_t &percent) {
    for (const char *p = message; *p != '\0' && *p != ';' && *p != '*'; ++p) {
        if (*p != '%') {
            continue;
        }

        const char *start = p;
        while (start > message && start[-1] >= '0' && start[-1] <= '9') {
            --start;
        }
        if (start == p) {
            continue;
        }

        char *end = nullptr;
        const auto parsed = strtol(start, &end, 10);
        if (end == p && parsed >= 0 && parsed <= 100) {
            percent = parsed;
            return true;
        }
    }

    return false;
}

bool parse_eta_minutes(const char *message, uint32_t &minutes) {
    const char *eta = find_case_insensitive(message, "eta");
    if (!eta) {
        return false;
    }

    eta += 3;
    while (*eta == ' ' || *eta == ':' || *eta == '=') {
        ++eta;
    }

    char *end = nullptr;
    const auto hour = strtol(eta, &end, 10);
    if (end == eta || *end != ':') {
        return false;
    }

    const char *minute_start = end + 1;
    const auto minute = strtol(minute_start, &end, 10);
    if (end == minute_start || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    const time_t now = time(nullptr);
    tm now_tm {};
    if (!localtime_r(&now, &now_tm)) {
        return false;
    }

    const int now_minutes = now_tm.tm_hour * 60 + now_tm.tm_min;
    const int eta_minutes = hour * 60 + minute;
    int delta = eta_minutes - now_minutes;
    if (delta < 0) {
        delta += 24 * 60;
    }

    minutes = std::max(delta, 1);
    return true;
}

bool parse_duration_seconds_after(const char *message, const char *marker, uint32_t &seconds) {
    const char *duration = find_case_insensitive(message, marker);
    if (!duration) {
        return false;
    }

    duration += strlen(marker);
    while (*duration == ' ' || *duration == ':' || *duration == '=') {
        ++duration;
    }

    long values[3] = {};
    size_t value_count = 0;
    const char *p = duration;
    while (value_count < 3) {
        char *end = nullptr;
        values[value_count] = strtol(p, &end, 10);
        if (end == p || values[value_count] < 0) {
            break;
        }
        ++value_count;
        if (*end != ':') {
            break;
        }
        p = end + 1;
    }

    switch (value_count) {
    case 1:
        seconds = values[0] * 60;
        return true;
    case 2:
        seconds = values[0] * 60 + values[1];
        return true;
    case 3:
        seconds = values[0] * 60 * 60 + values[1] * 60 + values[2];
        return true;
    default:
        return false;
    }
}

bool parse_etl_seconds(const char *message, uint32_t &seconds) {
    return parse_duration_seconds_after(message, "etl", seconds)
        || parse_duration_seconds_after(message, "time left", seconds)
        || parse_duration_seconds_after(message, "remaining", seconds);
}

void parse_octoprint_status_message(const char *command) {
    if (!command_starts_with(command, 'M', 117)) {
        return;
    }

    const char *message = command_arg(command);
    M73_Params params;

    uint8_t percent = 0;
    if (parse_percent(message, percent)) {
        params.standard_mode.percentage = percent;
    }

    uint32_t remaining_seconds = 0;
    if (parse_etl_seconds(message, remaining_seconds)) {
        params.standard_mode.time_to_end = remaining_seconds;
    } else {
        uint32_t eta_minutes = 0;
        if (parse_eta_minutes(message, eta_minutes)) {
            params.standard_mode.time_to_end = eta_minutes * 60;
        }
    }

    if (params != M73_Params {}) {
        M73_PE_no_parser(params);
    }
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
        parse_octoprint_status_message(command);
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

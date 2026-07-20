#include <serial_printing.hpp>
#include <state/printer_state.hpp>
#include <option/developer_mode.h>
#include <config_store/store_instance.hpp>
#include <printer_lock.hpp>
#include "../Marlin/src/core/serial.h"
#include "../Marlin/src/gcode/lcd/M73_PE.h"
#include <option/has_side_leds.h>
#if HAS_SIDE_LEDS()
    #include <leds/side_strip_handler.hpp>
#endif
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <option/has_wastebin_fill_tracking.h>
#if HAS_WASTEBIN_FILL_TRACKING()
    #include <feature/wastebin_watcher/wastebin_watcher.hpp>
#endif

uint32_t SerialPrinting::last_serial_indicator_ms = 0;
uint8_t SerialPrinting::last_host_progress_percent = 0;
uint32_t SerialPrinting::last_host_progress_ms = 0;
uint32_t SerialPrinting::last_host_time_to_end_s = 0;
uint32_t SerialPrinting::last_host_time_to_end_ms = 0;
uint32_t SerialPrinting::status_message_baseline_id = 0;
uint32_t SerialPrinting::last_status_hash = 0;
int8_t SerialPrinting::last_status_progress = -1;
uint32_t SerialPrinting::last_status_notification_ms = 0;

void SerialPrinting::set_status_message_baseline(uint32_t id) {
    status_message_baseline_id = id;
}

uint32_t SerialPrinting::status_message_baseline() {
    return status_message_baseline_id;
}

void SerialPrinting::reset_status_notifications() {
    last_status_hash = 0;
    last_status_progress = -1;
    last_status_notification_ms = 0;
}

void SerialPrinting::notify_status(const char *message, int progress_percent, bool force) {
    if (!marlin_server::serial_print_active() || message == nullptr || message[0] == '\0') {
        return;
    }

    uint32_t hash = 2166136261u;
    for (const char *c = message; *c; ++c) {
        hash = (hash ^ static_cast<uint8_t>(*c)) * 16777619u;
    }
    progress_percent = std::clamp(progress_percent, -1, 100);
    const uint32_t now = ticks_ms();
    const bool changed = hash != last_status_hash;
    const bool useful_progress = progress_percent >= 0
        && (last_status_progress < 0 || std::abs(progress_percent - last_status_progress) >= 5);
    if (!force && !changed && !useful_progress
        && ticks_diff(now, last_status_notification_ms) < 5000) {
        return;
    }

    SERIAL_ECHOPGM("//action:notification ");
    SERIAL_ECHO(message);
    if (progress_percent >= 0) {
        SERIAL_ECHOPAIR(" ", progress_percent);
        SERIAL_CHAR('%');
    }
    SERIAL_EOL();
    last_status_hash = hash;
    last_status_progress = static_cast<int8_t>(progress_percent);
    last_status_notification_ms = now;
}

void SerialPrinting::host_action(const char *action, const char *reason) {
    SERIAL_ECHOPGM("//action:");
    SERIAL_ECHO(action);
    if (reason != nullptr && reason[0] != '\0') {
        SERIAL_CHAR(' ');
        SERIAL_ECHO(reason);
    }
    SERIAL_EOL();
}

void SerialPrinting::print_loop() {
    if (has_serial_timeouted()) {
        marlin_server::serial_print_finalize();
    }
}

void SerialPrinting::abort() {
    host_action("cancel");
}

void SerialPrinting::resume(bool notify_host) {
    last_serial_indicator_ms = ticks_ms();
    GCodeQueue::pause_serial_commands = false;
    if (notify_host) {
        host_action("resume");
    }
}

void SerialPrinting::pause(bool notify_host) {
    if (notify_host) {
        host_action("pause");
    }
}

void SerialPrinting::paused(const char *reason) {
    host_action("paused", reason);
}

void SerialPrinting::resumed() {
    host_action("resumed");
}

bool SerialPrinting::has_serial_timeouted() {
    auto curr_time { ticks_ms() };
    if (GCodeQueue::has_commands_queued() || planner.processing()) {
        // refresh timer if there are commands still processing, we want to timeout after command finishes, not after when its queued
        last_serial_indicator_ms = curr_time;
        return false;
    }

    if (last_serial_indicator_ms != 0 && last_serial_indicator_ms <= curr_time && curr_time <= last_serial_indicator_ms + serial_printing_screen_timeout_ms()) {
        return false;
    } else {
        return true;
    }
}

uint32_t SerialPrinting::serial_printing_screen_timeout_ms() {
    return static_cast<uint32_t>(std::max<uint16_t>(1, config_store().serial_print_timeout_s.get())) * 1000;
}

void SerialPrinting::reset_host_progress() {
    last_host_progress_ms = 0;
    last_host_progress_percent = 0;
    last_host_time_to_end_ms = 0;
    last_host_time_to_end_s = 0;
}

void SerialPrinting::set_host_progress_percent(uint8_t percent) {
    last_host_progress_percent = percent;
    last_host_progress_ms = ticks_ms();
}

bool SerialPrinting::host_progress_percent(uint8_t &percent, uint32_t now_ms) {
    if (last_host_progress_ms == 0) {
        return false;
    }

    const uint32_t validity_ms = std::max<uint32_t>(serial_printing_screen_timeout_ms(), 60 * 1000);
    if (ticks_diff(now_ms, last_host_progress_ms + validity_ms) > 0) {
        return false;
    }

    percent = last_host_progress_percent;
    return true;
}

void SerialPrinting::set_host_time_to_end(uint32_t seconds) {
    last_host_time_to_end_s = seconds;
    last_host_time_to_end_ms = ticks_ms();
}

bool SerialPrinting::host_time_to_end(uint32_t &seconds, uint32_t now_ms) {
    if (last_host_time_to_end_ms == 0) {
        return false;
    }

    const uint32_t validity_ms = std::max<uint32_t>(serial_printing_screen_timeout_ms(), 60 * 1000);
    if (ticks_diff(now_ms, last_host_time_to_end_ms + validity_ms) > 0) {
        return false;
    }

    const auto elapsed_ms = ticks_diff(now_ms, last_host_time_to_end_ms);
    const uint32_t elapsed_s = elapsed_ms > 0 ? elapsed_ms / 1000 : 0;
    seconds = elapsed_s < last_host_time_to_end_s ? last_host_time_to_end_s - elapsed_s : 0;
    return true;
}

SerialPrintingUiMode SerialPrinting::ui_mode() {
    const auto mode = config_store().serial_print_ui_mode.get();
    if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(SerialPrintingUiMode::_last)) {
        return SerialPrintingUiMode::progress;
    }
    return mode;
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

bool has_flag_param(const char *command, char param) {
    // Parameters are whitespace-delimited for the commands handled here. Stop at G-code comments
    // and checksums so an explanatory comment cannot accidentally trigger an out-of-band action.
    for (const char *p = command; *p != '\0' && *p != ';' && *p != '*'; ++p) {
        if (*p == param && (p == command || p[-1] == ' ' || p[-1] == '\t')) {
            return true;
        }
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

bool m73_print_start(const char *command) {
    if (!m73_progress_is(command, 0)) {
        return false;
    }

    int remaining = 0;
    return find_param_int(command, 'R', remaining) || find_param_int(command, 'S', remaining);
}

bool m73_finished(const char *command) {
    if (!m73_progress_is(command, 100)) {
        return false;
    }

    int remaining = 0;
    return (find_param_int(command, 'R', remaining) || find_param_int(command, 'S', remaining)) && remaining == 0;
}

bool blocking_startup_gcode(const char *command) {
    return command_starts_with(command, 'M', 109) // wait for hotend
        || command_starts_with(command, 'M', 190) // wait for bed
        || command_starts_with(command, 'M', 191); // wait for chamber
}

bool print_start_gcode(const char *command) {
    return command_starts_with(command, 'M', 75) || (config_store().serial_print_auto_start.get() && (m73_print_start(command) || blocking_startup_gcode(command)));
}

bool print_end_gcode(const char *command) {
    // M73 P100 R0 is progress telemetry and can arrive before all streamed end
    // gcode has been sent. Treat only M77 as an explicit print-end marker; hosts
    // without M77 still finish through the serial inactivity timeout.
    return command_starts_with(command, 'M', 77);
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

        const char *number_end = p;
        while (number_end > message && number_end[-1] == ' ') {
            --number_end;
        }

        const char *start = number_end;
        while (start > message && ((start[-1] >= '0' && start[-1] <= '9') || start[-1] == '.')) {
            --start;
        }
        if (start == number_end) {
            continue;
        }

        char *end = nullptr;
        const auto parsed = strtof(start, &end);
        if (end == number_end && parsed >= 0.0f && parsed <= 100.0f) {
            percent = static_cast<uint8_t>(std::min<float>(std::round(parsed), 100.0f));
            return true;
        }
    }

    return false;
}

void parse_octoprint_status_message(const char *command);

void parse_serial_host_progress(const char *command) {
    parse_octoprint_status_message(command);
}

bool parse_clock_seconds_after(const char *message, const char *marker, uint32_t &seconds_until) {
    const char *clock = find_case_insensitive(message, marker);
    if (!clock) {
        return false;
    }

    clock += strlen(marker);
    while (*clock == ' ' || *clock == ':' || *clock == '=') {
        ++clock;
    }

    char *end = nullptr;
    auto hour = strtol(clock, &end, 10);
    if (end == clock || *end != ':') {
        return false;
    }

    const char *minute_start = end + 1;
    auto minute = strtol(minute_start, &end, 10);
    if (end == minute_start || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    long second = 0;
    if (*end == ':') {
        const char *second_start = end + 1;
        second = strtol(second_start, &end, 10);
        if (end == second_start || second < 0 || second > 59) {
            return false;
        }
    }

    while (*end == ' ') {
        ++end;
    }

    if ((end[0] == 'P' || end[0] == 'p') && (end[1] == 'M' || end[1] == 'm')) {
        if (hour >= 1 && hour < 12) {
            hour += 12;
        }
    } else if ((end[0] == 'A' || end[0] == 'a') && (end[1] == 'M' || end[1] == 'm')) {
        if (hour == 12) {
            hour = 0;
        }
    }

    const time_t now = time(nullptr);
    tm now_tm {};
    if (!localtime_r(&now, &now_tm)) {
        return false;
    }

    const int now_seconds = now_tm.tm_hour * 60 * 60 + now_tm.tm_min * 60 + now_tm.tm_sec;
    const int target_seconds = hour * 60 * 60 + minute * 60 + second;
    int delta = target_seconds - now_seconds;
    if (delta < 0) {
        delta += 24 * 60 * 60;
    }

    seconds_until = std::max(delta, 1);
    return true;
}

bool parse_eta_seconds(const char *message, uint32_t &seconds) {
    if (parse_clock_seconds_after(message, "ets", seconds)) {
        return true;
    }

    return parse_clock_seconds_after(message, "eta", seconds);
}

bool parse_eta_minutes(const char *message, uint32_t &minutes) {
    uint32_t seconds = 0;
    if (!parse_eta_seconds(message, seconds)) {
        return false;
    }
    minutes = std::max<uint32_t>(seconds / 60, 1);
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

    {
        const char *p = duration;
        uint32_t parsed_seconds = 0;
        bool found_unit = false;
        while (*p != '\0') {
            while (*p == ' ') {
                ++p;
            }

            char *end = nullptr;
            const auto value = strtol(p, &end, 10);
            if (end == p || value < 0) {
                break;
            }

            while (*end == ' ') {
                ++end;
            }

            uint32_t multiplier = 0;
            if (*end == 'h' || *end == 'H') {
                multiplier = 60 * 60;
            } else if (*end == 'm' || *end == 'M') {
                multiplier = 60;
            } else if (*end == 's' || *end == 'S') {
                multiplier = 1;
            } else {
                break;
            }

            parsed_seconds += static_cast<uint32_t>(value) * multiplier;
            found_unit = true;
            p = end + 1;
        }

        if (found_unit) {
            seconds = parsed_seconds;
            return true;
        }
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

bool octoprint_status_print_start(const char *command) {
    if (!command_starts_with(command, 'M', 117)) {
        return false;
    }

    const char *message = command_arg(command);

    uint8_t percent = 0;
    if (!parse_percent(message, percent) || percent != 0) {
        return false;
    }

    uint32_t seconds = 0;
    return parse_etl_seconds(message, seconds) || parse_eta_seconds(message, seconds);
}

void parse_octoprint_status_message(const char *command) {
    if (!command_starts_with(command, 'M', 117)) {
        return;
    }

    const char *message = command_arg(command);
    M73_Params params;

    uint8_t percent = 0;
    // A repeated startup-style "0%" host message is not useful progress data.
    // Leave streamed M73 P/R values in control until the host reports progress.
    if (parse_percent(message, percent) && percent > 0) {
        params.standard_mode.percentage = percent;
        SerialPrinting::set_host_progress_percent(percent);
    }

    uint32_t remaining_seconds = 0;
    if (parse_etl_seconds(message, remaining_seconds)) {
        params.standard_mode.time_to_end = remaining_seconds;
        SerialPrinting::set_host_time_to_end(remaining_seconds);
    } else {
        uint32_t eta_seconds = 0;
        if (parse_eta_seconds(message, eta_seconds)) {
            params.standard_mode.time_to_end = eta_seconds;
            SerialPrinting::set_host_time_to_end(eta_seconds);
        }
    }

    if (params != M73_Params {}) {
        M73_PE_no_parser(params);
    }
}

bool SerialPrinting::serial_command_hook(const char *command) {
    // never enter serial printing in developer mode as it breaks live debugging
    if (option::developer_mode) {
        return true;
    }

    remove_N_prefix(command);

#if HAS_WASTEBIN_FILL_TRACKING()
    // The full-bin handler blocks normal G-code execution to preserve the print queue. Consume the
    // reset and subsequent host resume directly from the serial receive path so they cannot sit
    // behind that queue. All other M1986/M602 commands retain their normal behavior.
    if (command_starts_with(command, 'M', 1986)) {
        if (has_flag_param(command, 'R') && WastebinWatcher::instance().serial_reset_and_hold_for_resume()) {
            auto &watcher = WastebinWatcher::instance();
            SERIAL_ECHOLNPAIR("PURGE_BUCKET pellets=", watcher.fill_level(),
                " threshold=", watcher.pause_threshold(), " capacity=", watcher.capacity());
            SERIAL_ECHOLNPGM("ok");
            return false;
        }
    }
    if (command_starts_with(command, 'M', 602)
        && WastebinWatcher::instance().serial_resume_after_reset()) {
        SERIAL_ECHOLNPGM("ok");
        return false;
    }
#endif

#if HAS_SIDE_LEDS()
    // Host polling/progress updates can arrive continuously while the printer is idle.
    // Treat real serial commands as activity, but don't let passive status traffic keep
    // the LEDs awake forever.
    if (command[0] != '\0'
        && command[0] != ';'
        && !command_starts_with(command, 'M', 27)
        && !command_starts_with(command, 'M', 73)
        && !command_starts_with(command, 'M', 105)
        && !command_starts_with(command, 'M', 114)
        && !command_starts_with(command, 'M', 115)
        && !command_starts_with(command, 'M', 117)
        && !command_starts_with(command, 'M', 153)
        && !command_starts_with(command, 'M', 155)) {
        leds::SideStripHandler::instance().activity_ping();
    }
#endif

    if (marlin_server::serial_print_active()) {
        last_serial_indicator_ms = ticks_ms();
        parse_serial_host_progress(command);
        if (print_end_gcode(command)) {
#if HAS_SIDE_LEDS()
            leds::SideStripHandler::instance().print_finished_ping();
#endif
            marlin_server::serial_print_finalize();
            SERIAL_ECHOLNPGM("ok");
            return false;
        }
        return true;
    }

    if (print_start_gcode(command) || octoprint_status_print_start(command)) {
        // When a host streams startup G-code, blocking heater waits such as M109 or M190 can be in progress
        // before M75/M73 arrives. That is still a serial print start, not an unrelated busy state.
        if (!printer_state::remote_print_ready(true)) {
            if (printer_state::get_state(false) != printer_state::DeviceState::Busy || marlin_server::is_printing()) {
                return true;
            }
        }

        if (!printer_lock::serial_print_start_allowed()) {
            return false;
        }
        last_serial_indicator_ms = ticks_ms();
        reset_host_progress();
        marlin_server::serial_print_start();
        parse_serial_host_progress(command);
    }

    return true;
}

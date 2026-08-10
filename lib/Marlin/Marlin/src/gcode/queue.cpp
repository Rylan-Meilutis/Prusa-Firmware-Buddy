/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * queue.cpp - The G-code command queue
 */

#include "queue.h"
GCodeQueue queue;

#include "gcode.h"

#include "../lcd/ultralcd.h"
#include "../module/planner.h"
#include "../module/temperature.h"
#include "../Marlin.h"
#include "serial_printing.hpp"
#include "marlin_server.hpp"
#include <gcode/inject_queue.hpp>
#include <feature/cork/tracker.hpp>
#include <serial_remote_control.hpp>
#include <config_store/store_instance.hpp>
#include <filament.hpp>
#include <filament_manufacturer.hpp>
#include <printer_lock.hpp>
#include <odometer.hpp>
#if ENABLED(PRUSA_TOOL_MAPPING)
  #include "../module/prusa/tool_mapper.hpp"
  extern void rme_report_tool_mapping();
#endif
#if __has_include(<option/has_indx.h>)
  #include <option/has_indx.h>
  #define RME_HAS_INDX() HAS_INDX()
#else
  #define RME_HAS_INDX() 0
#endif
#include <option/has_mmu2.h>
#include <option/has_toolchanger.h>
#include <option/has_chamber_filtration_api.h>
#if __has_include(<option/has_wastebin_fill_tracking.h>)
  #include <option/has_wastebin_fill_tracking.h>
  #define RME_HAS_WASTEBIN_FILL_TRACKING() HAS_WASTEBIN_FILL_TRACKING()
#else
  #define RME_HAS_WASTEBIN_FILL_TRACKING() 0
#endif
#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

extern "C" bool buddy_sdcard_upload_active();
extern "C" void buddy_sdcard_upload_handle_line(const char *command);
extern "C" bool buddy_sdcard_upload_start_command(const char *command);
extern "C" void buddy_sdcard_upload_finish_command();
extern "C" bool buddy_rme_file_service(const char *command);
extern "C" bool buddy_rme_binary_upload_active();
extern "C" void buddy_rme_binary_upload_byte(uint8_t byte);

/**
 * GCode line number handling. Hosts may opt to include line numbers when
 * sending commands to Marlin, and lines will be checked for sequentiality.
 * M110 N<int> sets the current line number.
 */
long gcode_N, GCodeQueue::last_N, GCodeQueue::stopped_N = 0;

/**
 * GCode Command Queue
 * A simple ring buffer of BUFSIZE command strings.
 *
 * Commands are copied into this buffer by the command injectors
 * (immediate, serial, sd card) and they are processed sequentially by
 * the main loop. The gcode.process_next_command method parses the next
 * command and hands off execution to individual handler functions.
 */
uint8_t GCodeQueue::length = 0,  // Count of commands in the queue
        GCodeQueue::index_r = 0, // Ring buffer read position
        GCodeQueue::index_w = 0; // Ring buffer write position

char GCodeQueue::command_buffer[GCodeQueue::recovery_capacity][MAX_CMD_SIZE];

uint32_t GCodeQueue::sdpos = GCodeQueue::SDPOS_INVALID;
uint32_t GCodeQueue::last_executed_sdpos = GCodeQueue::SDPOS_INVALID;
uint32_t GCodeQueue::executed_commmand_count = 0;
uint32_t GCodeQueue::sdpos_buffer[GCodeQueue::recovery_capacity];
bool GCodeQueue::current_command_serial = false;
bool GCodeQueue::pause_serial_commands = false;

/*
 * The port that the command was received on
 */
#if NUM_SERIAL > 1
  // #error dead code found by automatic analyses (see BFW-5461)
  int16_t GCodeQueue::port[GCodeQueue::recovery_capacity];
#endif

/**
 * Serial command injection
 */

// Number of characters read in the current line of serial input
static int serial_count[NUM_SERIAL] = { 0 };

bool send_ok[GCodeQueue::recovery_capacity];

/**
 * Next Injected Command pointer. nullptr if no commands are being injected.
 * Used by Marlin internally to ensure that commands initiated from within
 * are enqueued ahead of any pending serial or sd card commands.
 */
static PGM_P injected_commands_P = nullptr;

GCodeQueue::GCodeQueue() {
  // Send "ok" after commands by default
  for (uint8_t i = 0; i < COUNT(send_ok); i++) send_ok[i] = true;
}

/**
 * Check whether there are any commands yet to be executed
 */
bool GCodeQueue::has_commands_queued() {
  return queue.length || injected_commands_P;
}

bool GCodeQueue::current_command_from_serial() {
  return current_command_serial;
}

/**
 * Clear the Marlin command queue
 */
void GCodeQueue::clear() {
  sdpos = last_executed_sdpos;
  index_r = index_w = length = 0;
  buddy::cork::tracker.clear();
}

/**
 * Once a new command is in the ring buffer, call this to commit it
 */
void GCodeQueue::_commit_command(bool say_ok
  #if NUM_SERIAL > 1
    , int16_t p/*=-1*/
  #endif
) {
  send_ok[index_w] = say_ok;
  #if NUM_SERIAL > 1
    port[index_w] = p;
  #endif
  sdpos_buffer[index_w] = sdpos;

  if (++index_w >= recovery_capacity) index_w = 0;
  length++;
}

/**
 * Copy a command from RAM into the main command buffer.
 * Return true if the command was successfully added.
 * Return false for a full buffer, or if the 'command' is a comment.
 */
bool GCodeQueue::_enqueue(const char* cmd, bool say_ok/*=false*/
  #if NUM_SERIAL > 1
    , int16_t pn/*=-1*/
  #endif
) {
  if (*cmd == ';' || length >= recovery_capacity) return false;
  strcpy(command_buffer[index_w], cmd);
  _commit_command(say_ok
    #if NUM_SERIAL > 1
      , pn
    #endif
  );
  return true;
}

/**
 * Enqueue with Serial Echo
 * Return true if the command was consumed
 */
 [[nodiscard]] bool GCodeQueue::enqueue_one(const char* cmd, bool echo/*=true*/) {

  //SERIAL_ECHOPGM("enqueue_one(\"");
  //SERIAL_ECHO(cmd);
  //SERIAL_ECHOPGM("\") \n");

  if (*cmd == 0 || *cmd == '\n' || *cmd == '\r' || *cmd == ';') return true;

  if (_enqueue(cmd)) {
    if (echo) {
        SERIAL_ECHO_START();
        SERIAL_ECHOLNPAIR(MSG_ENQUEUEING, cmd, "\"");
    }
    return true;
  }
  return false;
}

/**
 * Process the next "immediate" command.
 * Return 'true' if any commands were processed,
 * or remain to process.
 */
bool GCodeQueue::process_injected_command() {
  if (injected_commands_P == nullptr) {
    const auto inject_gcode = inject_queue.get_gcode();
    if (inject_gcode.has_value()) {
      // successfully received G-Code stream [const char *]
      injected_commands_P = *inject_gcode;
    } else if (inject_gcode.error() == InjectQueue::GetGCodeError::empty) {
      // Empty inject_queue -> continue parsing standard G-Code queue
      return false;
    } else {
      // Inject G-Code is not ready yet (buffering from file)
      // or loading error occurred, in both cases skip standard G-Code queue
      return true;
    }
  }

  char c;
  size_t i = 0;
  while ((c = pgm_read_byte(&injected_commands_P[i])) && c != '\n') i++;

  // Extract current command and move pointer to next command
  char cmd[i + 1];
  memcpy_P(cmd, injected_commands_P, i);
  cmd[i] = '\0';
  injected_commands_P = c ? injected_commands_P + i + 1 : nullptr;

  // Execute command if non-blank
  if (i) {
    parser.parse(cmd);
    PORT_REDIRECT(SERIAL_PORT);
    gcode.process_parsed_command();
  }
  return true;
}

/**
 * Enqueue one or many commands to inject_queue, to run from program memory.
 * Do not inject a comment or use leading spaces!
 * G-Codes are enqueued only if inject_queue is not already full
 * Note: process_injected_command() will be called to drain any commands afterwards
 */
void GCodeQueue::inject_P(ConstexprString pgcode) { inject(GCodeLiteral(pgcode)); }

/**
 * Enqueue action to inject_queue, if inject_queue isn't already full
 */
bool GCodeQueue::inject(InjectQueueRecord request) { 
  return inject_queue.try_push(request);
}

/**
 * Enqueue and return only when commands are actually enqueued.
 * Never call this from a G-code handler!
 */
void GCodeQueue::enqueue_one_now(const char* cmd) { while (!enqueue_one(cmd)) idle(true); }

/**
 * Enqueue from program memory and return only when commands are actually enqueued
 * Never call this from a G-code handler!
 */
void GCodeQueue::enqueue_now_P(PGM_P const pgcode) {
  size_t i = 0;
  PGM_P p = pgcode;
  for (;;) {
    char c;
    while ((c = pgm_read_byte(&p[i])) && c != '\n') i++;
    char cmd[i + 1];
    memcpy_P(cmd, p, i);
    cmd[i] = '\0';
    enqueue_one_now(cmd);
    if (!c) break;
    p += i + 1;
  }
}

/**
 * Send an "ok" message to the host, indicating
 * that a command was successfully processed.
 *
 * If ADVANCED_OK is enabled also include:
 *   N<int>  Line number of the command, if any
 *   P<int>  Planner space remaining
 *   B<int>  Block queue space remaining
 */
void GCodeQueue::ok_to_send() {
  #if NUM_SERIAL > 1
    const int16_t pn = port[index_r];
    if (pn < 0) return;
    PORT_REDIRECT(pn);
  #endif
  if (!send_ok[index_r]) return;
  SERIAL_ECHOPGM(MSG_OK);
  #if ENABLED(ADVANCED_OK)
    char* p = command_buffer[index_r];
    if (*p == 'N') {
      SERIAL_ECHO(' ');
      SERIAL_ECHO(*p++);
      while (NUMERIC_SIGNED(*p))
        SERIAL_ECHO(*p++);
    }
    SERIAL_ECHOPGM(" P"); SERIAL_ECHO(int(BLOCK_BUFFER_SIZE - planner.movesplanned() - 1));
    // Recovery reserve slots are intentionally hidden from normal host flow
    // control. Never advertise them as additional streaming capacity.
    SERIAL_ECHOPGM(" B"); SERIAL_ECHO(length < BUFSIZE ? BUFSIZE - length : 0);
  #endif
  SERIAL_EOL();
}

/**
 * Send a "Resend: nnn" message to the host to
 * indicate that a command needs to be re-sent.
 */
void GCodeQueue::flush_and_request_resend() {
  #if NUM_SERIAL > 1
    const int16_t p = port[index_r];
    if (p < 0) return;
    PORT_REDIRECT(p);
  #endif
  SERIAL_FLUSH();
  SERIAL_ECHOPGM(MSG_RESEND);
  SERIAL_ECHOLN(last_N + 1);
  ok_to_send();
}

inline bool serial_data_available() {
  return false
    || MYSERIAL0.available()
    #if NUM_SERIAL > 1
      || MYSERIAL1.available()
    #endif
  ;
}

inline int read_serial(const uint8_t index) {
  switch (index) {
    case 0: return MYSERIAL0.read();
    #if NUM_SERIAL > 1
      case 1: return MYSERIAL1.read();
    #endif
    default: return -1;
  }
}

void GCodeQueue::gcode_line_error(PGM_P const err, const int8_t port) {
  PORT_REDIRECT(port);
  SERIAL_ERROR_START();
  serialprintPGM(err);
  SERIAL_ECHOLN(last_N);
  while (read_serial(port) != -1);           // clear out the RX buffer
  flush_and_request_resend();
  serial_count[port] = 0;
}

FORCE_INLINE bool is_M29(const char * const cmd) {  // matches "M29" & "M29 ", but not "M290", etc
  const char * const m29 = strstr_P(cmd, PSTR("M29"));
  return m29 && !NUMERIC(m29[3]);
}

static bool command_code_is(const char *cmd, const char letter, const long code) {
  while (*cmd == ' ') cmd++;
  if (*cmd == 'N') {
    cmd++;
    while (*cmd == '-' || NUMERIC(*cmd)) cmd++;
    while (*cmd == ' ') cmd++;
  }

  if (*cmd != letter) return false;

  char *end = nullptr;
  const long parsed = strtol(cmd + 1, &end, 10);
  return end != cmd + 1 && parsed == code && (*end == '\0' || *end == ' ' || *end == '*');
}

static std::optional<long> numbered_command_line(const char *cmd) {
  while (*cmd == ' ') cmd++;
  if (*cmd++ != 'N') return std::nullopt;
  char *end = nullptr;
  const long line = strtol(cmd, &end, 10);
  return end != cmd ? std::optional<long> { line } : std::nullopt;
}

static bool is_print_abort_command(const char *cmd) {
  return command_code_is(cmd, 'M', 604) || command_code_is(cmd, 'M', 524);
}

static bool is_priority_service_command(const char *command) {
  if (command_code_is(command, 'M', 108)
      || command_code_is(command, 'M', 112)
      || command_code_is(command, 'M', 410)
      || command_code_is(command, 'M', 601)
      || command_code_is(command, 'M', 602)
      || is_print_abort_command(command)) {
    return true;
  }
  if (command_code_is(command, 'M', 876)) {
    return strchr(command, 'Q') || strchr(command, 'S') || strchr(command, 'A');
  }
  if (command_code_is(command, 'M', 1601)) {
    return strchr(command, 'C') || strchr(command, 'U') || strchr(command, 'A');
  }
  return false;
}

static bool response_name_equal(const std::string_view requested, const char *canonical) {
  const std::string_view expected { canonical };
  return requested.size() == expected.size()
    && std::equal(requested.begin(), requested.end(), expected.begin(), [](const char lhs, const char rhs) {
         return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
       });
}

static std::optional<std::string_view> command_string_parameter(const char *command, const char parameter) {
  const char *value = strchr(command, parameter);
  if (!value) {
    return std::nullopt;
  }
  value++;
  while (*value == ' ') value++;
  const bool quoted = *value == '"';
  if (quoted) value++;
  const char *end = value;
  while (*end && *end != '*' && (quoted ? *end != '"' : *end != ' ')) end++;
  if (end == value || (quoted && *end != '"')) {
    return std::nullopt;
  }
  return std::string_view { value, static_cast<size_t>(end - value) };
}

static bool dialog_blocks_generic_resume() {
  return marlin_vars().peek_fsm_states([](const fsm::States &states) {
    const auto top = states.get_top();
    if (!top) {
      return false;
    }
    if (top->fsm_type == ClientFSM::Printing || top->fsm_type == ClientFSM::Serial_printing) {
      return false;
    }
    const auto &responses = ClientResponses::get_fsm_responses(top->fsm_type, top->data.GetPhase());
    return std::any_of(responses.begin(), responses.end(), [](const Response response) {
      return response != Response::_none;
    });
  });
}

static const char *command_payload(const char *command) {
  while (*command == ' ') command++;
  if (*command == 'N') {
    command++;
    while (*command == '-' || NUMERIC(*command)) command++;
    while (*command == ' ') command++;
  }
  return command;
}

static bool handle_remote_ui_service(const char *command) {
  command = command_payload(command);
  constexpr char prefix[] = "@RME UI ";
  if (strncmp(command, prefix, sizeof(prefix) - 1) != 0) {
    return false;
  }
  command += sizeof(prefix) - 1;

  if (strncmp(command, "ENABLE ", 7) == 0) {
    const char *enable = command + 7;
    char *end = nullptr;
    const long value = strtol(enable, &end, 10);
    if (end != enable && (value == 0 || value == 1)) {
      serial_remote_control::set_enabled(value == 1);
    }
    return true;
  }

  serial_remote_control::Action action;
  int16_t value = 0;
  if (strncmp(command, "ENCODER ", 8) == 0) {
    const char *jog = command + 8;
    char *end = nullptr;
    const long parsed = strtol(jog, &end, 10);
    if (end == jog || parsed < -100 || parsed > 100 || parsed == 0) {
      return true;
    }
    action = serial_remote_control::Action::encoder;
    value = static_cast<int16_t>(parsed);
  } else if (strncmp(command, "CLICK", 5) == 0) {
    action = serial_remote_control::Action::click;
  } else if (strncmp(command, "BACK", 4) == 0) {
    action = serial_remote_control::Action::back;
  } else if (strncmp(command, "HOME", 4) == 0) {
    action = serial_remote_control::Action::home;
  } else {
    return false;
  }

  serial_remote_control::enqueue(action, value);
  return true;
}

static std::optional<std::string_view> remote_value(const std::string_view command, const std::string_view key) {
  size_t token = command.find(' ');
  while (token != std::string_view::npos) {
    ++token;
    const size_t end = command.find_first_of(" *", token);
    const size_t token_size = (end == std::string_view::npos ? command.size() : end) - token;
    if (token_size > key.size() && command[token + key.size()] == '=' && command.substr(token, key.size()) == key)
      return command.substr(token + key.size() + 1, token_size - key.size() - 1);
    token = end;
  }
  return std::nullopt;
}

static std::optional<long> remote_number(const std::string_view command, const std::string_view key, const int base = 10) {
  const auto value = remote_value(command, key);
  if (!value || value->empty() || value->size() >= 16) return std::nullopt;
  char buffer[16] {};
  std::copy(value->begin(), value->end(), buffer);
  char *end = nullptr;
  const char *start = buffer;
  if (base == 16 && *start == '#') start++;
  const long result = strtol(start, &end, base);
  return end != start && *end == '\0' ? std::optional<long> { result } : std::nullopt;
}

static void report_remote_lock() {
  SERIAL_ECHOPGM("RME_LOCK enabled=");
  SERIAL_ECHO(printer_lock::enabled() ? 1 : 0);
  SERIAL_ECHOPGM(" locked=");
  SERIAL_ECHOLN(printer_lock::locked() ? 1 : 0);
}

static bool handle_remote_lock_service(const std::string_view command) {
  constexpr std::string_view prefix = "@RME LOCK ";
  if (!command.starts_with(prefix)) return false;
  const auto action = command.substr(prefix.size());
  if (action.starts_with("QUERY")) {
    report_remote_lock();
  } else if (action.starts_with("NOW")) {
    printer_lock::lock();
    serial_remote_control::set_enabled(false);
  } else if (action.starts_with("UNLOCK")) {
    const auto pin = remote_number(command, "pin");
    const auto digits = remote_number(command, "digits");
    if (pin && digits && *digits >= 4 && *digits <= 9 && printer_lock::check_pin(*pin, *digits)) {
      printer_lock::unlock();
    }
  } else if (action.starts_with("SET")) {
    if (printer_lock::locked()) {
      return true;
    }
    const auto pin = remote_number(command, "pin");
    const auto digits = remote_number(command, "digits");
    if (pin || digits) {
      if (!pin || !digits || *digits < 4 || *digits > 9) {
        return true;
      }
      config_store().printer_lock_pin.set(*pin);
      config_store().printer_lock_pin_length.set(*digits);
    }
    if (const auto timeout = remote_number(command, "timeout"); timeout && *timeout >= 0 && *timeout <= 65535)
      config_store().printer_lock_timeout_s.set(*timeout);
    if (const auto serial = remote_number(command, "serial"); serial && (*serial == 0 || *serial == 1))
      config_store().printer_lock_accept_serial.set(*serial);
    if (const auto enabled = remote_number(command, "enabled"); enabled && (*enabled == 0 || *enabled == 1)) {
      if (!*enabled || config_store().printer_lock_pin_length.get() >= 4)
        config_store().printer_lock_enabled.set(*enabled);
    }
  } else return false;
  return true;
}

static void report_remote_theme() {
  SERIAL_ECHOPGM("RME_THEME primary="); SERIAL_ECHO(config_store().ui_theme_primary_color.get());
  SERIAL_ECHOPGM(" progress="); SERIAL_ECHO(config_store().ui_theme_progress_color.get());
  SERIAL_ECHOPGM(" warning="); SERIAL_ECHO(config_store().ui_theme_warning_color.get());
  SERIAL_ECHOPGM(" error="); SERIAL_ECHO(config_store().ui_theme_error_color.get());
  SERIAL_ECHOPGM(" image="); SERIAL_ECHOLN(config_store().ui_theme_image_color.get());
}

template <typename StoreItem>
static void set_remote_color(const std::string_view command, const std::string_view key, StoreItem &item) {
  if (const auto value = remote_number(command, key, 16); value && *value >= 0 && *value <= 0xffffff)
    item.set(*value);
}

static bool handle_remote_theme_service(const std::string_view command) {
  constexpr std::string_view prefix = "@RME THEME ";
  if (!command.starts_with(prefix)) return false;
  const auto action = command.substr(prefix.size());
  if (action.starts_with("QUERY")) {
    report_remote_theme();
  } else if (action.starts_with("SET")) {
    if (printer_lock::locked()) {
      return true;
    }
    set_remote_color(command, "primary", config_store().ui_theme_primary_color);
    set_remote_color(command, "progress", config_store().ui_theme_progress_color);
    set_remote_color(command, "warning", config_store().ui_theme_warning_color);
    set_remote_color(command, "error", config_store().ui_theme_error_color);
    set_remote_color(command, "image", config_store().ui_theme_image_color);
    serial_remote_control::reload_theme();
  } else return false;
  return true;
}

static bool handle_remote_light_service(const std::string_view command) {
  constexpr std::string_view prefix = "@RME LIGHT ";
  if (!command.starts_with(prefix)) return false;
  const auto action = command.substr(prefix.size());
  if (action.starts_with("QUERY")) {
    const uint32_t screen = config_store().screen_brightness_by_state.get();
    SERIAL_ECHOPGM("RME_LIGHT screen_persistent=");
    SERIAL_ECHO((screen >> 24) & 0xff);
    const auto lights = serial_remote_control::light_status();
    SERIAL_ECHOPGM(" chamber_print=");
    SERIAL_ECHO(lights.print_chamber);
    SERIAL_ECHOPGM(" screen_print=");
    SERIAL_ECHO(lights.print_screen);
    SERIAL_ECHOPGM(" status_print=");
    SERIAL_ECHO(lights.print_status);
    SERIAL_EOL();
  } else if (action.starts_with("TEMP")) {
    const auto screen = remote_number(command, "screen").value_or(-1);
    const auto chamber = remote_number(command, "chamber").value_or(-1);
    const auto status = remote_number(command, "status").value_or(-1);
    serial_remote_control::set_temporary_lights(screen, chamber, status);
  } else if (action.starts_with("SET")) {
    if (printer_lock::locked()) {
      return true;
    }
    const auto screen = remote_number(command, "screen", 16);
    const auto chamber = remote_number(command, "chamber", 16);
    const auto status = remote_number(command, "status", 16);
    if (screen && chamber && status)
      serial_remote_control::set_persistent_lights(*screen, *chamber, *status);
  } else return false;
  return true;
}

static void report_remote_filaments() {
  constexpr size_t total = preset_filament_type_count + user_filament_type_count;
  for (size_t i = 0; i < total; ++i) {
    const bool user = i >= preset_filament_type_count;
    const size_t slot = user ? i - preset_filament_type_count : i;
    const FilamentType type = user
      ? FilamentType { UserFilamentType { static_cast<uint8_t>(slot) } }
      : FilamentType { static_cast<PresetFilamentType>(slot) };
    const auto params = type.parameters();
    SERIAL_ECHO("RME_FILAMENT user="); SERIAL_ECHO(user ? 1 : 0);
    SERIAL_ECHO(" slot="); SERIAL_ECHO(slot);
    SERIAL_ECHO(" name="); SERIAL_ECHO(params.name.data());
    SERIAL_ECHO(" nozzle="); SERIAL_ECHO(params.nozzle_temperature);
    SERIAL_ECHO(" preheat="); SERIAL_ECHO(params.nozzle_preheat_temperature);
    SERIAL_ECHO(" bed="); SERIAL_ECHO(params.heatbed_temperature);
#if HAS_FILAMENT_BASE_PRESET_PARAM
    SERIAL_ECHO(" base=");
    if (params.base_preset) SERIAL_ECHO(FilamentType { *params.base_preset }.parameters().name.data());
    else SERIAL_ECHO("none");
#endif
#if HAS_FILAMENT_HEATBREAK_PARAM()
    SERIAL_ECHO(" heatbreak="); SERIAL_ECHO(params.heatbreak_temperature);
#endif
#if HAS_CHAMBER_API()
    SERIAL_ECHO(" chamber_min="); SERIAL_ECHO(params.chamber_min_temperature.value_or(-1));
    SERIAL_ECHO(" chamber_max="); SERIAL_ECHO(params.chamber_max_temperature.value_or(-1));
    SERIAL_ECHO(" chamber_target="); SERIAL_ECHO(params.chamber_target_temperature.value_or(-1));
    SERIAL_ECHO(" filtration="); SERIAL_ECHO(params.requires_filtration ? 1 : 0);
#endif
    SERIAL_ECHO(" abrasive="); SERIAL_ECHO(params.is_abrasive ? 1 : 0);
    SERIAL_ECHO(" flexible="); SERIAL_ECHOLN(params.do_not_auto_retract ? 1 : 0);
  }
}

static bool handle_remote_filament_service(const std::string_view command) {
  constexpr std::string_view prefix = "@RME FILAMENT ";
  if (!command.starts_with(prefix)) return false;
  const auto action = command.substr(prefix.size());
  if (action.starts_with("QUERY")) {
    report_remote_filaments();
  } else if (action.starts_with("SET") || action.starts_with("CREATE")) {
    if (printer_lock::locked()) {
      return true;
    }
    const auto slot = remote_number(command, "slot");
    if (!slot || *slot < 0 || static_cast<size_t>(*slot) >= user_filament_type_count) {
      return true;
    }
    const FilamentType type = UserFilamentType { static_cast<uint8_t>(*slot) };
    auto params = type.parameters();
    if (const auto name = remote_value(command, "name"); name && name->size() < filament_name_buffer_size && type.can_be_renamed_to(*name)) {
      params.name = {};
      std::copy(name->begin(), name->end(), params.name.begin());
    }
    if (const auto value = remote_number(command, "nozzle")) params.nozzle_temperature = *value;
    if (const auto value = remote_number(command, "preheat")) params.nozzle_preheat_temperature = *value;
    if (const auto value = remote_number(command, "bed")) params.heatbed_temperature = *value;
#if HAS_FILAMENT_BASE_PRESET_PARAM
    // Some upstream hosts call this material family a brand. Accept both wire
    // names, but persist it in the upstream base_preset field used by the UI.
    auto base = remote_value(command, "base");
    if (!base) base = remote_value(command, "brand");
    if (base) {
      if (*base == "none") {
        params.base_preset = std::nullopt;
      } else {
        const auto base_type = FilamentType::from_name(*base);
        if (const auto preset = std::get_if<PresetFilamentType>(&base_type)) {
          params.base_preset = *preset;
        } else {
          SERIAL_ECHOLNPGM("echo:RME_ERROR code=invalid_filament_base");
          return true;
        }
      }
    }
#endif
#if HAS_FILAMENT_HEATBREAK_PARAM()
    if (const auto value = remote_number(command, "heatbreak")) params.heatbreak_temperature = *value;
#endif
#if HAS_CHAMBER_API()
    const auto set_optional_temperature = [&](const std::string_view key, auto &field) {
      if (const auto value = remote_number(command, key))
        field = *value < 0 ? std::nullopt : std::optional<uint8_t> { static_cast<uint8_t>(*value) };
    };
    set_optional_temperature("chamber_min", params.chamber_min_temperature);
    set_optional_temperature("chamber_max", params.chamber_max_temperature);
    set_optional_temperature("chamber_target", params.chamber_target_temperature);
    if (const auto value = remote_number(command, "filtration"); value && (*value == 0 || *value == 1)) params.requires_filtration = *value;
#endif
    if (const auto value = remote_number(command, "abrasive"); value && (*value == 0 || *value == 1)) params.is_abrasive = *value;
    if (const auto value = remote_number(command, "flexible"); value && (*value == 0 || *value == 1)) params.do_not_auto_retract = *value;
    type.set_parameters(params);
    if (const auto visible = remote_number(command, "visible"); visible && (*visible == 0 || *visible == 1)) type.set_visible(*visible);
  } else return false;
  return true;
}

static std::optional<std::array<char, filament_manufacturer::name_capacity>> remote_manufacturer_name(const std::string_view command) {
  const auto encoded = remote_value(command, "name");
  if (!encoded || encoded->empty()) return std::nullopt;
  std::array<char, filament_manufacturer::name_capacity> result {};
  size_t out = 0;
  const auto hex = [](const char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    const char lower = std::tolower(static_cast<unsigned char>(c));
    return lower >= 'a' && lower <= 'f' ? lower - 'a' + 10 : -1;
  };
  for (size_t i = 0; i < encoded->size() && out + 1 < result.size(); ++i) {
    if ((*encoded)[i] == '%' && i + 2 < encoded->size()) {
      const int hi = hex((*encoded)[i + 1]), lo = hex((*encoded)[i + 2]);
      if (hi < 0 || lo < 0) return std::nullopt;
      result[out++] = static_cast<char>((hi << 4) | lo);
      i += 2;
    } else result[out++] = (*encoded)[i];
  }
  return result;
}

static void report_remote_manufacturer_name(const std::string_view name) {
  constexpr char hex[] = "0123456789ABCDEF";
  for (const unsigned char c : name) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.') SERIAL_CHAR(c);
    else { SERIAL_CHAR('%'); SERIAL_CHAR(hex[c >> 4]); SERIAL_CHAR(hex[c & 0xf]); }
  }
}

static bool handle_remote_manufacturer_service(const std::string_view command) {
  constexpr std::string_view prefix = "@RME MANUFACTURER ";
  if (!command.starts_with(prefix)) return false;
  const auto action = command.substr(prefix.size());
  if (action.starts_with("QUERY")) {
    size_t slot = 0;
    for (const auto name : filament_manufacturer::presets()) {
      SERIAL_ECHOPGM("RME_MANUFACTURER builtin=1 slot="); SERIAL_ECHO(slot++); SERIAL_ECHOPGM(" name=");
      report_remote_manufacturer_name(name); SERIAL_EOL();
    }
    for (size_t i = 0; i < filament_manufacturer::custom_slot_count; ++i) if (const auto item = filament_manufacturer::custom(i)) {
      SERIAL_ECHOPGM("RME_MANUFACTURER builtin=0 slot="); SERIAL_ECHO(i); SERIAL_ECHOPGM(" name=");
      report_remote_manufacturer_name(item->name_view()); SERIAL_EOL();
    }
    for (uint8_t tool = 0; tool < EXTRUDERS; ++tool) {
      SERIAL_ECHOPGM("RME_MANUFACTURER_LOADED tool="); SERIAL_ECHO(tool); SERIAL_ECHOPGM(" name=");
      if (const auto item = filament_manufacturer::loaded(tool)) report_remote_manufacturer_name(item->name_view());
      else SERIAL_ECHOPGM("none");
      SERIAL_EOL();
    }
  } else if (action.starts_with("CREATE")) {
    if (printer_lock::locked()) return true;
    const auto slot = remote_number(command, "slot");
    const auto name = remote_manufacturer_name(command);
    if (!slot || *slot < 0 || *slot >= static_cast<long>(filament_manufacturer::custom_slot_count) || !name
        || !filament_manufacturer::set_custom(*slot, name->data()))
      SERIAL_ECHOLNPGM("echo:RME_ERROR code=invalid_manufacturer");
  } else if (action.starts_with("DELETE")) {
    if (printer_lock::locked()) return true;
    const auto slot = remote_number(command, "slot");
    if (!slot || *slot < 0 || !filament_manufacturer::clear_custom(*slot)) SERIAL_ECHOLNPGM("echo:RME_ERROR code=invalid_manufacturer_slot");
  } else if (action.starts_with("ASSIGN")) {
    if (printer_lock::locked()) return true;
    const auto tool = remote_number(command, "tool");
    const auto name = remote_manufacturer_name(command);
    if (!tool || *tool < 0 || *tool >= EXTRUDERS || !name) return true;
    const auto decoded_name = std::string_view(name->data());
    const bool is_none = decoded_name.size() == 4 && std::equal(decoded_name.begin(), decoded_name.end(), "none", [](const char lhs, const char rhs) {
      return std::tolower(static_cast<unsigned char>(lhs)) == rhs;
    });
    if (is_none) filament_manufacturer::set_loaded(*tool, std::nullopt);
    else if (const auto item = filament_manufacturer::find(name->data())) filament_manufacturer::set_loaded(*tool, item->id);
    else SERIAL_ECHOLNPGM("echo:RME_ERROR code=unknown_manufacturer");
  } else return false;
  return true;
}

static bool handle_remote_machine_service(const std::string_view command) {
  constexpr std::string_view query = "@RME MACHINE QUERY";
  if (!command.starts_with("@RME MACHINE ")) return false;
  if (!command.starts_with(query)) return false;

  SERIAL_ECHOPGM("RME_MACHINE hotends="); SERIAL_ECHO(HOTENDS);
  SERIAL_ECHOPGM(" logical_tools="); SERIAL_ECHO(EXTRUDERS);
  SERIAL_ECHOPGM(" single_nozzle="); SERIAL_ECHOLN(HOTENDS == 1 ? 1 : 0);

  SERIAL_ECHOPGM("RME_ENVELOPE x_min="); SERIAL_ECHO(X_MIN_POS);
  SERIAL_ECHOPGM(" x_max="); SERIAL_ECHO(X_MAX_POS);
  SERIAL_ECHOPGM(" y_min="); SERIAL_ECHO(Y_MIN_POS);
  SERIAL_ECHOPGM(" y_max="); SERIAL_ECHO(Y_MAX_POS);
  SERIAL_ECHOPGM(" z_min="); SERIAL_ECHO(Z_MIN_POS);
  SERIAL_ECHOPGM(" z_max="); SERIAL_ECHOLN(Z_MAX_POS);

  SERIAL_ECHOPGM("RME_LIMITS feed_x="); SERIAL_ECHO(planner.settings.max_feedrate_mm_s[X_AXIS]);
  SERIAL_ECHOPGM(" feed_y="); SERIAL_ECHO(planner.settings.max_feedrate_mm_s[Y_AXIS]);
  SERIAL_ECHOPGM(" feed_z="); SERIAL_ECHOLN(planner.settings.max_feedrate_mm_s[Z_AXIS]);
  return true;
}

static bool handle_remote_stats_service(const std::string_view command) {
  constexpr std::string_view query = "@RME STATS QUERY";
  if (!command.starts_with("@RME STATS ")) return false;
  if (!command.starts_with(query)) return false;

  auto &odometer = Odometer_s::instance();
  const float distance_x_m = odometer.get_axis(Odometer_s::axis_t::X);
  const float distance_y_m = odometer.get_axis(Odometer_s::axis_t::Y);
  const float distance_z_m = odometer.get_axis(Odometer_s::axis_t::Z);

  SERIAL_ECHOPGM("RME_STATS distance_x_m="); SERIAL_ECHO(distance_x_m);
  SERIAL_ECHOPGM(" distance_y_m="); SERIAL_ECHO(distance_y_m);
  SERIAL_ECHOPGM(" distance_z_m="); SERIAL_ECHO(distance_z_m);
  SERIAL_ECHOPGM(" distance_total_m="); SERIAL_ECHO(distance_x_m + distance_y_m + distance_z_m);
  SERIAL_ECHOPGM(" extruded_m="); SERIAL_ECHO(odometer.get_extruded_all());
  SERIAL_ECHOPGM(" print_time_s="); SERIAL_ECHO(odometer.get_time());
  SERIAL_ECHOPGM(" current_print_time_s="); SERIAL_ECHO(marlin_vars().print_duration.get());
  SERIAL_ECHOPGM(" jobs_started="); SERIAL_ECHOLN(config_store().job_id.get());

  SERIAL_ECHOPGM("RME_STATS_OPERATIONS tool_picks="); SERIAL_ECHO(odometer.get_toolpick_all());
  SERIAL_ECHOPGM(" mmu_changes="); SERIAL_ECHO(odometer.get_mmu_changes());
#if HAS_CHAMBER_FILTRATION_API()
  SERIAL_ECHOPGM(" filtering_time_s="); SERIAL_ECHO(config_store().chamber_filter_time_used_s.get());
#else
  SERIAL_ECHOPGM(" filtering_time_s=0");
#endif
#if RME_HAS_WASTEBIN_FILL_TRACKING()
  SERIAL_ECHOPGM(" wastebin_pellets="); SERIAL_ECHO(odometer.get_nozzle_cleaner_pellets());
#endif
  SERIAL_EOL();

  SERIAL_ECHOPGM("RME_STATS_FAILURES crash_x="); SERIAL_ECHO(config_store().crash_count_x.get());
  SERIAL_ECHOPGM(" crash_y="); SERIAL_ECHO(config_store().crash_count_y.get());
  SERIAL_ECHOPGM(" power_panics="); SERIAL_ECHO(config_store().power_panics_count.get());
#if HAS_MMU2()
  SERIAL_ECHOPGM(" mmu_load_since_reset="); SERIAL_ECHO(config_store().mmu2_load_fails.get());
  SERIAL_ECHOPGM(" mmu_load_total="); SERIAL_ECHO(config_store().mmu2_total_load_fails.get());
  SERIAL_ECHOPGM(" mmu_general_since_reset="); SERIAL_ECHO(config_store().mmu2_fails.get());
  SERIAL_ECHOPGM(" mmu_general_total="); SERIAL_ECHO(config_store().mmu2_total_fails.get());
#endif
  SERIAL_EOL();
  return true;
}

static bool handle_dialog_service_response(const char *command);

static void report_remote_session() {
  SERIAL_ECHOPGM("RME_SESSION active="); SERIAL_ECHO(serial_remote_control::session_active() ? 1 : 0);
  SERIAL_ECHOPGM(" legacy="); SERIAL_ECHO(serial_remote_control::legacy_notifications_enabled() ? 1 : 0);
  SERIAL_ECHOLNPGM(" preferred_baud=1000000 fallback_baud=250000,230400,115200");
}

static bool handle_remote_session_service(const std::string_view command) {
  constexpr std::string_view prefix = "@RME SESSION ";
  if (!command.starts_with(prefix)) return false;
  const auto action = command.substr(prefix.size());
  if (action.starts_with("OPEN")) {
    const auto events = remote_number(command, "events").value_or(0x0f);
    const auto legacy = remote_number(command, "legacy").value_or(0);
    if (events < 0 || events > 0x0f || (legacy != 0 && legacy != 1)) {
      SERIAL_ECHOLNPGM("echo:RME_ERROR code=invalid_session_options");
      return true;
    }
    serial_remote_control::open_session(static_cast<uint8_t>(events), legacy == 1);
    report_remote_session();
  } else if (action.starts_with("KEEPALIVE")) {
    serial_remote_control::keepalive_session();
    report_remote_session();
  } else if (action.starts_with("QUERY")) {
    report_remote_session();
  } else if (action.starts_with("CLOSE")) {
    serial_remote_control::close_session();
    report_remote_session();
  } else return false;
  return true;
}

static bool handle_remote_toolmap_service(const std::string_view command) {
  if (!command.starts_with("@RME TOOLMAP ")) return false;
#if ENABLED(PRUSA_TOOL_MAPPING)
  const auto action = command.substr(13);
  if (action.empty()) return false;
  if (action[0] == 'Q') {
    rme_report_tool_mapping();
  } else if (action[0] == 'S') {
    const auto logical = remote_number(command, "logical");
    const auto physical = remote_number(command, "physical");
    if (logical && physical && *logical >= 0 && *logical < EXTRUDERS && *physical >= 0 && *physical < EXTRUDERS)
      tool_mapper.set_mapping(*logical, *physical);
  } else if (action[0] == 'E') {
    if (const auto value = remote_number(command, "value"); value && (*value == 0 || *value == 1)) tool_mapper.set_enable(*value);
  } else if (action[0] == 'R') {
    tool_mapper.reset();
  } else {
    return false;
  }
  return true;
#else
  SERIAL_ECHOLNPGM("echo:RME_ERROR code=unsupported feature=tool_mapping");
  return true;
#endif
}

static bool handle_remote_service_frame(const char *raw_command) {
  const char *payload = command_payload(raw_command);
  if (strncmp(payload, "@RME ", 5) != 0) return false;
  const std::string_view command { payload, strcspn(payload, "*") };
  if (handle_remote_ui_service(raw_command)
      || handle_remote_session_service(command)
      || handle_remote_lock_service(command)
      || handle_remote_theme_service(command)
      || handle_remote_light_service(command)
      || handle_remote_filament_service(command)
      || handle_remote_manufacturer_service(command)
      || handle_remote_machine_service(command)
      || handle_remote_stats_service(command)
      || handle_remote_toolmap_service(command)
      || buddy_rme_file_service(payload)
      || handle_dialog_service_response(payload)) return true;
  SERIAL_ECHOLNPGM("echo:RME_ERROR unknown");
  return true;
}

static void report_service_queue_status() {
  SERIAL_ECHOPGM("RME_PROMPT ");
  bool first = true;
  marlin_vars().peek_fsm_states([&](const fsm::States &states) {
    const auto top = states.get_top();
    if (!top) {
      return;
    }
    const auto &responses = ClientResponses::get_fsm_responses(top->fsm_type, top->data.GetPhase());
    for (const Response response : responses) {
      if (response == Response::_none) continue;
      if (!first) SERIAL_CHAR(',');
      SERIAL_ECHOPGM(to_str(response));
      first = false;
    }
  });
  if (first) SERIAL_ECHOPGM("none");
  SERIAL_EOL();
}

static bool handle_dialog_service_response(const char *command) {
  const bool gcode = command_code_is(command, 'M', 876);
  const char *options = command;
  if (!gcode) {
    const char *payload = command_payload(command);
    constexpr char dialog_prefix[] = "@RME DIALOG ";
    constexpr char stuck_prefix[] = "@RME STUCK ";
    if (strncmp(payload, dialog_prefix, sizeof(dialog_prefix) - 1) == 0) options = payload + sizeof(dialog_prefix) - 1;
    else if (strncmp(payload, stuck_prefix, sizeof(stuck_prefix) - 1) == 0) options = payload + sizeof(stuck_prefix) - 1;
    else return false;
  }

  if (strncmp(options, "QUERY", 5) == 0 || (gcode && strchr(options, 'Q'))) {
    report_service_queue_status();
    return true;
  }
  if (!gcode && strncmp(options, "RESPOND ", 8) == 0) options += 8;

  if (!gcode && *options == '\0') {
    return false;
  }

  const char *spos = strchr(options, 'S');
  auto requested_name = command_string_parameter(options, 'A');
  if (!gcode && !requested_name && !spos && *options) requested_name = std::string_view { options, strcspn(options, " *") };
  if (!spos && !requested_name) {
    return false;
  }

  long response_index = -1;
  if (spos) {
    char *end = nullptr;
    response_index = strtol(spos + 1, &end, 10);
    if (end == spos + 1 || response_index < 0) {
      SERIAL_ECHOLNPGM("echo:M876 requires a non-negative S button index");
      return true;
    }
  }

  std::optional<EncodedFSMResponse> encoded_response;
  marlin_vars().peek_fsm_states([&](const fsm::States &states) {
    const auto top = states.get_top();
    if (!top) {
      return;
    }
    const auto &responses = ClientResponses::get_fsm_responses(top->fsm_type, top->data.GetPhase());
    Response response = Response::_none;
    if (requested_name) {
      const auto match = std::find_if(responses.begin(), responses.end(), [&](const Response candidate) {
        return candidate != Response::_none && response_name_equal(*requested_name, to_str(candidate));
      });
      if (match != responses.end()) response = *match;
    } else {
      if (static_cast<size_t>(response_index) >= responses.size()) {
        return;
      }
      response = responses[response_index];
    }
    if (response == Response::_none) {
      return;
    }
    encoded_response = EncodedFSMResponse {
      .response = FSMResponseVariant::make(response),
      .fsm_and_phase = FSMAndPhase(top->fsm_type, top->data.GetPhase()),
    };
  });

  if (!encoded_response) {
    SERIAL_ECHOLNPGM("echo:M876 response ignored: no matching active dialog action");
    return true;
  }
  marlin_server::set_response(*encoded_response);
  return true;
}

#if HAS_LOADCELL()
static bool handle_stuck_filament_response(const char *command) {
  if (!command_code_is(command, 'M', 1601)) return false;

  const bool choose_continue = strchr(command, 'C');
  const bool choose_unload = strchr(command, 'U');
  const bool choose_abort = strchr(command, 'A');
  const uint8_t choices = uint8_t(choose_continue) + uint8_t(choose_unload) + uint8_t(choose_abort);
  if (!choices) return false; // Bare M1601 starts the internal recovery flow.

  if (choices != 1) {
    SERIAL_ECHOLNPGM("echo:M1601 response requires exactly one of C, U, or A");
    return true;
  }

  const bool recovery_active = marlin_vars().peek_fsm_states([](const fsm::States &states) {
    return states.is_active(ClientFSM::Load_unload)
      && states[ClientFSM::Load_unload]->GetPhase() == std::to_underlying(PhasesLoadUnload::FilamentStuck);
  });
  if (!recovery_active) {
    SERIAL_ECHOLNPGM("echo:M1601 response ignored: no stuck-filament prompt is active");
    return true;
  }

  const Response response = choose_continue ? Response::Continue
    : choose_unload                  ? Response::Unload
                                     : Response::Abort;
  marlin_server::set_response(EncodedFSMResponse {
    .response = FSMResponseVariant::make(response),
    .fsm_and_phase = PhasesLoadUnload::FilamentStuck,
  });
  return true;
}
#endif

/**
 * Get all commands waiting on the serial port and queue them.
 * Exit when the buffer is full or when no more characters are
 * left on the serial port.
 */
void GCodeQueue::get_serial_commands() {
  // RME bulk frames are consumed out-of-band and never copied into the
  // MAX_CMD_SIZE G-code queue.
  static constexpr size_t rme_serial_line_size = 640;
  static char serial_line_buffer[NUM_SERIAL][rme_serial_line_size];
  static bool serial_comment_mode[NUM_SERIAL] = { false }
              #if ENABLED(PAREN_COMMENTS)
                , serial_comment_paren_mode[NUM_SERIAL] = { false }
              #endif
            ;

  // If the command buffer is empty for too long,
  // send "wait" to indicate Marlin is still waiting.
  #if NO_TIMEOUTS > 0
    static millis_t last_command_time = 0;
    const millis_t ms = millis();
    if (length == 0 && !serial_data_available() && ELAPSED(ms, last_command_time + NO_TIMEOUTS)) {
      SERIAL_ECHOLNPGM(MSG_WAIT);
      last_command_time = ms;
    }
  #endif

  /**
   * Loop while serial characters are incoming and the queue is not full
   */
  // Always drain up to the hidden recovery capacity. A blocking foreground
  // command may not have raised a pause/error FSM yet, but the host must still
  // be able to submit heat-wait cancellation, pause, abort, and service G-codes.
  while ((buddy_rme_binary_upload_active() || length < recovery_capacity) && serial_data_available()) {
    for (uint8_t i = 0; i < NUM_SERIAL; ++i) {
      int c;
      if ((c = read_serial(i)) < 0) continue;

      if (buddy_rme_binary_upload_active()) {
        buddy_rme_binary_upload_byte(static_cast<uint8_t>(c));
        continue;
      }

      char serial_char = c;

      /**
       * If the character ends the line
       */
      if (serial_char == '\n' || serial_char == '\r') {

        // Start with comment mode off
        serial_comment_mode[i] = false;
        #if ENABLED(PAREN_COMMENTS)
          serial_comment_paren_mode[i] = false;
        #endif

        // Skip empty lines and comments
        if (!serial_count[i]) { thermalManager.manage_heater(); continue; }

        serial_line_buffer[i][serial_count[i]] = 0;       // Terminate string
        serial_count[i] = 0;                              // Reset buffer

        char* command = serial_line_buffer[i];

        while (*command == ' ') command++;                // Skip leading spaces
        char *npos = (*command == 'N') ? command : nullptr;  // Require the N parameter to start the line

        if (npos) {

          bool M110 = strstr_P(command, PSTR("M110")) != nullptr;

          if (M110) {
            char* n2pos = strchr(command + 4, 'N');
            if (n2pos) npos = n2pos;
          }

          gcode_N = strtol(npos + 1, nullptr, 10);

          // Validate the retransmitted bytes before treating an older line as
          // an already accepted command. This preserves checksum diagnostics
          // while allowing hosts to recover when the command's final `ok` was
          // lost among asynchronous status/action messages.
          char *apos = strrchr(command, '*');
          if (apos) {
            uint8_t checksum = 0, count = uint8_t(apos - command);
            while (count) checksum ^= command[--count];
            if (strtol(apos + 1, nullptr, 10) != checksum)
              return gcode_line_error(PSTR(MSG_ERR_CHECKSUM_MISMATCH), i);
          }
          else
            return gcode_line_error(PSTR(MSG_ERR_NO_CHECKSUM), i);

          if (gcode_N != last_N + 1 && !M110) {
            // A host may retransmit a long-running numbered command before its
            // final ok. It is already executing, so do not flush RX or start a
            // Resend loop; acknowledge that processing is still alive instead.
            const auto executing_line = current_command_serial && length
              ? numbered_command_line(command_buffer[index_r])
              : std::nullopt;
            if (executing_line == gcode_N) {
              SERIAL_ECHO_MSG(MSG_BUSY_PROCESSING);
              continue;
            }
            // Marlin records a numbered line when it accepts it into the
            // command queue, before execution produces the corresponding ok.
            // If that ok is lost, OctoPrint legitimately resends the accepted
            // line. Acknowledge it without executing it twice or flushing RX.
            if (gcode_N <= last_N) {
              SERIAL_ECHOLNPGM(MSG_OK);
              continue;
            }
            return gcode_line_error(PSTR(MSG_ERR_LINE_NO), i);
          }

          last_N = gcode_N;
        }

        // Movement commands alert when stopped
        if (IsStopped()) {
          char* gpos = strchr(command, 'G');
          if (gpos) {
            switch (strtol(gpos + 1, nullptr, 10)) {
              case 0:
              case 1:
              #if ENABLED(ARC_SUPPORT)
                case 2:
                case 3:
              #endif
              #if ENABLED(BEZIER_CURVE_SUPPORT)
                case 5:
              #endif
                SERIAL_ECHOLNPGM(MSG_ERR_STOPPED);
                LCD_MESSAGEPGM(MSG_STOPPED);
                break;
            }
          }
        }

        // Only this explicit safety/recovery whitelist may bypass normal FIFO
        // execution. Ordinary commands already in flight may occupy a hidden
        // receive slot, but never gain priority semantics.
        const bool priority_service = is_priority_service_command(command);

        #if DISABLED(EMERGENCY_PARSER)
          // Process critical commands early
          if (priority_service && command_code_is(command, 'M', 108)) {
            wait_for_heatup = false;
          }
          if (priority_service && command_code_is(command, 'M', 112)) kill(PSTR("Emergency stop (M112)"), nullptr, true);
          if (priority_service && command_code_is(command, 'M', 410)) quickstop_stepper();
          if (priority_service && is_print_abort_command(command)) {
            wait_for_heatup = false;
            marlin_server::print_abort();
            SERIAL_ECHOLNPGM(MSG_OK);
            continue;
          }
        #endif

        // A queued M601 would remain behind a blocking M109/M190/M191. Consume
        // serial pause immediately and release an active heater wait first.
        if (priority_service && command_code_is(command, 'M', 601)) {
          wait_for_heatup = false;
          marlin_server::print_pause(false);
          SERIAL_ECHOLNPGM(MSG_OK);
          continue;
        }

        // Resume is a priority service action only from a stable ordinary
        // pause. Recovery/error dialogs must be answered explicitly with M876
        // or their dedicated command instead of being bypassed by M602.
        if (priority_service && command_code_is(command, 'M', 602)) {
          if (marlin_server::printer_paused() && !dialog_blocks_generic_resume()) {
            marlin_server::print_resume(false);
          } else {
            SERIAL_ECHOLNPGM("echo:M602 ignored: printer is not safely resumable");
          }
          SERIAL_ECHOLNPGM(MSG_OK);
          continue;
        }

        #if HAS_LOADCELL()
          // M1601 owns the foreground G-code slot while its recovery FSM is
          // visible. Consume its C/U/A response directly from serial RX so a
          // paused host can act without waiting behind that blocking command.
          if (priority_service && handle_stuck_filament_response(command)) {
            SERIAL_ECHOLNPGM(MSG_OK);
            continue;
          }
        #endif

        // M876 S<n> selects the zero-based button from the currently visible
        // FSM dialog. Consume it directly so MMU Retry and other recovery
        // actions cannot be blocked behind the command that owns the normal
        // foreground queue.
        if (priority_service && handle_dialog_service_response(command)) {
          SERIAL_ECHOLNPGM(MSG_OK);
          continue;
        }

        // @RME frames are an out-of-band serial control protocol. They retain
        // the host's line/checksum accounting but never enter the print G-code
        // FIFO or manipulate screen objects from Marlin's serial task.
        if (handle_remote_service_frame(command)) {
          SERIAL_ECHOLNPGM(MSG_OK);
          continue;
        }

        #if defined(NO_TIMEOUTS) && NO_TIMEOUTS > 0
          last_command_time = ms;
        #endif

        if (buddy_sdcard_upload_active()) {
          if (is_M29(command)) {
            buddy_sdcard_upload_finish_command();
          }
          else {
            buddy_sdcard_upload_handle_line(command);
          }
          SERIAL_ECHOLNPGM(MSG_OK);
          continue;
        }

        if (command_code_is(command, 'M', 28)) {
          buddy_sdcard_upload_start_command(command);
          SERIAL_ECHOLNPGM(MSG_OK);
          continue;
        }

        // notify serial printing about command
        if (!SerialPrinting::serial_command_hook(command)) {
          continue;
        }

        // Add the command to the queue
        _enqueue(serial_line_buffer[i], true
          #if NUM_SERIAL > 1
            , i
          #endif
        );
      }
      else if (serial_count[i] >= static_cast<int>(rme_serial_line_size - 1)
        || (serial_count[i] >= MAX_CMD_SIZE - 1 && strncmp(serial_line_buffer[i], "@RME ", 5) != 0)) {
        // Keep fetching, but ignore normal characters beyond the max length
        // The command will be injected when EOL is reached
      }
      else if (serial_char == '\\') {  // Handle escapes
        // if we have one more character, copy it over
        if ((c = read_serial(i)) >= 0 && !serial_comment_mode[i]
          #if ENABLED(PAREN_COMMENTS)
            && !serial_comment_paren_mode[i]
          #endif
        )
          serial_line_buffer[i][serial_count[i]++] = (char)c;
      }
      else { // it's not a newline, carriage return or escape char
        if (serial_char == ';') serial_comment_mode[i] = true;
        #if ENABLED(PAREN_COMMENTS)
          else if (serial_char == '(') serial_comment_paren_mode[i] = true;
          else if (serial_char == ')') serial_comment_paren_mode[i] = false;
        #endif
        else if (!serial_comment_mode[i]
          #if ENABLED(PAREN_COMMENTS)
            && ! serial_comment_paren_mode[i]
          #endif
        ) serial_line_buffer[i][serial_count[i]++] = serial_char;
      }
    } // for NUM_SERIAL
  } // queue has space, serial has data
}

/**
 * Add to the circular command queue the next command from:
 *  - The command-injection queue (injected_commands_P)
 *  - The active serial input (usually USB)
 *  - The SD card file being actively printed
 */
void GCodeQueue::get_available_commands() {

  if (!pause_serial_commands)
    get_serial_commands();

}

/**
 * Get the next command in the queue, optionally log it to SD, then dispatch it
 */
void GCodeQueue::advance() {

  // Process immediate commands
  if (process_injected_command()) {
    delay(1); // Safety measure to avoid locking async job in inject queue (buffering a file)
    return;
  }

  // Return if the G-code buffer is empty
  if (!length) {
    delay(1);
    return;
  }

  last_executed_sdpos = queue.get_current_sdpos();
  current_command_serial = last_executed_sdpos == SDPOS_INVALID;
  gcode.process_next_command();
  current_command_serial = false;
  executed_commmand_count++;

  // The queue may be reset by a command handler or by code invoked by idle() within a handler
  if (length) {
    --length;
    if (++index_r >= recovery_capacity) index_r = 0;
  }

}

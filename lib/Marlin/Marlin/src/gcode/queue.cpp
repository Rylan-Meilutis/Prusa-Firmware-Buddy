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
#include <option/has_serial_print.h>
#if HAS_SERIAL_PRINT()
    #include "serial_printing.hpp"
#endif
#include "marlin_server.hpp"
#include <common/marlin_server_types/fsm/filament_change_phases.hpp>
#include <gcode/inject_queue.hpp>
#include <feature/cork/tracker.hpp>
#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

extern "C" bool buddy_sdcard_upload_active();
extern "C" void buddy_sdcard_upload_handle_line(const char *command);
extern "C" bool buddy_sdcard_upload_start_command(const char *command);
extern "C" void buddy_sdcard_upload_finish_command();

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
    // #error dead code found by automatic analyses (see BFW-5461)
    , int16_t p/*=-1*/
  #endif
) {
  send_ok[index_w] = say_ok;
  #if NUM_SERIAL > 1
    // #error dead code found by automatic analyses (see BFW-5461)
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
    // #error dead code found by automatic analyses (see BFW-5461)
    , int16_t pn/*=-1*/
  #endif
) {
  if (*cmd == ';' || length >= recovery_capacity) return false;
  strcpy(command_buffer[index_w], cmd);
  _commit_command(say_ok
    #if NUM_SERIAL > 1
      // #error dead code found by automatic analyses (see BFW-5461)
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
    // #error dead code found by automatic analyses (see BFW-5461)
    const int16_t pn = port[index_r];
    if (pn < 0) return;
    PORT_REDIRECT(pn);
  #endif
  if (!send_ok[index_r]) return;
  SERIAL_ECHOPGM(MSG_OK);
  #if ENABLED(ADVANCED_OK)
    // #error dead code found by automatic analyses (see BFW-5461)
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
    // #error dead code found by automatic analyses (see BFW-5461)
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
      // #error dead code found by automatic analyses (see BFW-5461)
      || MYSERIAL1.available()
    #endif
  ;
}

inline int read_serial(const uint8_t index) {
  switch (index) {
    case 0: return MYSERIAL0.read();
    #if NUM_SERIAL > 1
      // #error dead code found by automatic analyses (see BFW-5461)
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
    if (top->fsm_type == ClientFSM::Printing
        #if HAS_SERIAL_PRINT()
          || top->fsm_type == ClientFSM::Serial_printing
        #endif
    ) {
      return false;
    }
    const auto &responses = ClientResponses::get_fsm_responses(top->fsm_type, top->data.GetPhase());
    return std::any_of(responses.begin(), responses.end(), [](const Response response) {
      return response != Response::_none;
    });
  });
}

static void report_service_queue_status() {
  const uint8_t normal_used = std::min<uint8_t>(GCodeQueue::length, BUFSIZE);
  const uint8_t reserve_used = GCodeQueue::length > BUFSIZE ? GCodeQueue::length - BUFSIZE : 0;
  const bool paused = marlin_server::printer_paused();
  const bool resume_allowed = paused && !dialog_blocks_generic_resume();

  SERIAL_ECHOPGM("SERVICE_QUEUE normal=");
  SERIAL_ECHO(normal_used);
  SERIAL_CHAR('/');
  SERIAL_ECHO(BUFSIZE);
  SERIAL_ECHOPGM(" reserve=");
  SERIAL_ECHO(reserve_used);
  SERIAL_CHAR('/');
  SERIAL_ECHO(GCodeQueue::recovery_capacity - BUFSIZE);
  SERIAL_ECHOPGM(" state=");
  SERIAL_ECHO(static_cast<int>(marlin_vars().print_state.get()));
  SERIAL_ECHOPGM(" paused=");
  SERIAL_CHAR(paused ? '1' : '0');
  SERIAL_ECHOPGM(" resume=");
  SERIAL_CHAR(resume_allowed ? '1' : '0');
  SERIAL_ECHOPGM(" blocking=");
  if (GCodeQueue::length && GCodeQueue::current_command_serial) {
    const char *blocking = GCodeQueue::command_buffer[GCodeQueue::index_r];
    while (*blocking == ' ') blocking++;
    if (*blocking == 'N') {
      blocking++;
      while (*blocking == '-' || NUMERIC(*blocking)) blocking++;
      while (*blocking == ' ') blocking++;
    }
    while (*blocking && *blocking != ' ' && *blocking != '*') SERIAL_CHAR(*blocking++);
  } else {
    SERIAL_ECHOPGM("none");
  }
  SERIAL_ECHOPGM(" actions=");
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
  if (!command_code_is(command, 'M', 876)) {
    return false;
  }

  if (strchr(command, 'Q')) {
    report_service_queue_status();
    return true;
  }

  const char *spos = strchr(command, 'S');
  const auto requested_name = command_string_parameter(command, 'A');
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

#if HAS_LOADCELL() && HAS_EXTRUDER_FSENSOR()
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
  static char serial_line_buffer[NUM_SERIAL][MAX_CMD_SIZE];
  static bool serial_comment_mode[NUM_SERIAL] = { false }
              #if ENABLED(PAREN_COMMENTS)
                // #error dead code found by automatic analyses (see BFW-5461)
                , serial_comment_paren_mode[NUM_SERIAL] = { false }
              #endif
            ;

  // If the command buffer is empty for too long,
  // send "wait" to indicate Marlin is still waiting.
  #if NO_TIMEOUTS > 0
    // #error dead code found by automatic analyses (see BFW-5461)
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
  while (length < recovery_capacity && serial_data_available()) {
    for (uint8_t i = 0; i < NUM_SERIAL; ++i) {
      int c;
      if ((c = read_serial(i)) < 0) continue;

      char serial_char = c;

      /**
       * If the character ends the line
       */
      if (serial_char == '\n' || serial_char == '\r') {

        // Start with comment mode off
        serial_comment_mode[i] = false;
        #if ENABLED(PAREN_COMMENTS)
          // #error dead code found by automatic analyses (see BFW-5461)
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

        #if HAS_LOADCELL() && HAS_EXTRUDER_FSENSOR()
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

        #if defined(NO_TIMEOUTS) && NO_TIMEOUTS > 0
          // #error dead code found by automatic analyses (see BFW-5461)
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

#if HAS_SERIAL_PRINT()
        // notify serial printing about command
        if (!SerialPrinting::serial_command_hook(command)) {
          continue;
        }
#endif

        // Add the command to the queue
        _enqueue(serial_line_buffer[i], true
          #if NUM_SERIAL > 1
            // #error dead code found by automatic analyses (see BFW-5461)
            , i
          #endif
        );
      }
      else if (serial_count[i] >= MAX_CMD_SIZE - 1) {
        // Keep fetching, but ignore normal characters beyond the max length
        // The command will be injected when EOL is reached
      }
      else if (serial_char == '\\') {  // Handle escapes
        // if we have one more character, copy it over
        if ((c = read_serial(i)) >= 0 && !serial_comment_mode[i]
          #if ENABLED(PAREN_COMMENTS)
            // #error dead code found by automatic analyses (see BFW-5461)
            && !serial_comment_paren_mode[i]
          #endif
        )
          serial_line_buffer[i][serial_count[i]++] = (char)c;
      }
      else { // it's not a newline, carriage return or escape char
        if (serial_char == ';') serial_comment_mode[i] = true;
        #if ENABLED(PAREN_COMMENTS)
          // #error dead code found by automatic analyses (see BFW-5461)
          else if (serial_char == '(') serial_comment_paren_mode[i] = true;
          else if (serial_char == ')') serial_comment_paren_mode[i] = false;
        #endif
        else if (!serial_comment_mode[i]
          #if ENABLED(PAREN_COMMENTS)
            // #error dead code found by automatic analyses (see BFW-5461)
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

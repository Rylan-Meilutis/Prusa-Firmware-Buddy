"""Execute production pause/FIFO and G1 rejection code without printer motion."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(
    os.environ.get("RME_TEST_ROOT",
                   Path(__file__).resolve().parents[3]))


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


class SerialPauseRecovery(unittest.TestCase):

    def run_cpp(self, code):
        if not shutil.which("c++"):
            self.skipTest("C++ compiler required")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            for name in ("has_indx", "has_mmu2", "has_tool_crash_recovery",
                         "has_tool_mapping", "has_toolchanger"):
                (path / "option").mkdir(exist_ok=True)
                (path / "option" /
                 (name + ".h")).write_text("#define " + name.upper() +
                                           "() 1\n")
            (path / "test.cpp").write_text(code)
            subprocess.run([
                "c++", "-std=c++20", "-fsanitize=address,undefined",
                "-fno-pie", "-no-pie", "-I" + str(path),
                "-I" + str(ROOT / "src/common"),
                str(path / "test.cpp"), "-o",
                str(path / "test")
            ],
                           check=True)
            # LSan cannot run under this environment's ptrace sandbox; ASan
            # and UBSan remain enabled for all executed production paths.
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
            subprocess.run([str(path / "test")], check=True, env=env)

    def test_full_fifo_is_held_but_recovery_rx_stays_live(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        receive = function(source, "void GCodeQueue::get_available_commands()")
        advance = function(source, "void GCodeQueue::advance()")
        queued = function(source, "bool GCodeQueue::has_commands_queued()")
        processing = function(
            (ROOT / "src/common/marlin_server.cpp").read_text(),
            "bool is_processing()")
        code = r'''
#include <cassert>
#include <cstring>
#include <marlin_server_types/marlin_server_state.h>
#define HAS_SERIAL_PRINT() 1
#define HAS_SELFTEST() 0
namespace marlin_server {
State state = State::Printing;
struct { bool print_is_serial=true; State &print_state=state; } server;
bool serial_print_fifo_held() { return serial_print_fifo_held(true, state); }
}
void delay(int) {}
const char *injected_commands_P=nullptr;
int rx=0, executed=0;
constexpr unsigned SDPOS_INVALID=~0u;
struct GCodeQueue {
 static constexpr int recovery_capacity=10;
 int length=10, index_r=0;
 unsigned last_executed_sdpos=0;
 static inline bool pause_serial_commands=true;
 static inline bool current_command_serial=false;
 static inline int executed_commmand_count=0;
 bool process_injected_command() { return false; }
 bool has_commands_queued();
 unsigned get_current_sdpos() { return SDPOS_INVALID; }
 void get_serial_commands() { ++rx; }
 void get_available_commands(); void advance();
} queue;
struct GcodeSuite { static constexpr int NOT_BUSY=0; int busy_state=0; void process_next_command() { ++executed; } } gcode;
struct { bool processing() { return false; } } planner;
struct { bool is_empty() { return true; } } inject_queue;
'''
        main = r'''
int main() {
 using marlin_server::State;
 for(auto state: {State::Pausing_Begin, State::Pausing_WaitIdle, State::Pausing_ParkHead,
     State::Paused, State::Resuming_Begin, State::Resuming_Reheating,
     State::Resuming_UnparkHead_XY, State::Resuming_UnparkHead_ZE}) {
   marlin_server::state=state;
   assert(!marlin_server::is_processing());
   injected_commands_P="M600";
   assert(marlin_server::is_processing());
   injected_commands_P=nullptr;
   for(int i=0;i<1000;++i) { queue.get_available_commands(); queue.advance(); }
   assert(queue.length==10 && executed==0);
   assert(!marlin_server::serial_print_fifo_held(false,state));
 }
 assert(rx==8000);
 assert(!strcmp(marlin_server::serial_pause_phase(State::Pausing_WaitIdle),"pausing"));
 assert(!strcmp(marlin_server::serial_pause_phase(State::Paused),"paused"));
 assert(!strcmp(marlin_server::serial_pause_phase(State::Resuming_Reheating),"resuming"));
 marlin_server::state=State::Printing;
 for(int i=0;i<10;++i) queue.advance();
 assert(queue.length==0 && executed==10);
}
'''
        self.run_cpp("#include <initializer_list>\n" + code + queued +
                     receive + advance + "namespace marlin_server {" +
                     processing + "}\n" + main)

    def test_exact_crashed_retract_and_post_cancel_xyz(self):
        source = (ROOT /
                  "lib/Marlin/Marlin/src/gcode/motion/G0_G1.cpp").read_text()
        handler = function(source, "void GcodeSuite::G0_G1(")
        code = r'''
#include <cassert>
#include <optional>
#define TERN_(...)
#define HAS_CRASH_DETECTION() 0
#define HAS_INDX() 0
#define PRINTER_IS_PRUSA_XL() 0
#define HAS_CANCEL_OBJECT() 0
#define MOTION_CONDITIONS true
#define EXTRUDERS 1
#define ANY(...) 0
#define ENABLED(...) 0
#define SERIAL_ERROR_MSG(...) (++errors)
struct Pos { float x=242,y=205,z=225,e=218.049088f; } current_position, destination, requested;
bool selected=false;
int active_extruder=1;
struct PhysicalToolIndex { static std::optional<int> currently_selected_opt() { return selected ? std::optional<int>(0) : std::nullopt; } };
struct GCodeQueue { static bool current_command_from_serial() { return true; } };
struct { bool seen(char) { return false; } } parser;
float feedrate_mm_s=178.49;
int moves=0,errors=0;
struct Hints { struct { bool is_printing_move; } move; };
void prepare_move_to(Pos p,float,Hints) { ++moves; current_position=p; }
struct GcodeSuite {
 static void get_destination_from_command() { destination=requested; }
 static void G0_G1();
};
'''
        main = r'''
int main() {
 for(int i=0;i<1000;++i) {
  requested=current_position; requested.e-=6.5f;
  GcodeSuite::G0_G1();
  assert(moves==0 && current_position.e==218.049088f && destination.e==current_position.e);
 }
 assert(errors==1000);
 requested=current_position; requested.z=2.5f; GcodeSuite::G0_G1();
 assert(moves==1 && current_position.z==2.5f);
 selected=true; active_extruder=0; requested=current_position; requested.e-=6.5f; GcodeSuite::G0_G1();
 assert(moves==2 && current_position.e==211.549088f);
}
'''
        self.run_cpp(code + handler + main)

    def test_full_rx_preserves_numbered_watermark_and_checks_capacity(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        receive = function(source, "void GCodeQueue::get_serial_commands()")
        self.assertIn("while (serial_data_available())", receive)
        self.assertIn("last_N = accepted_line_before", receive)
        self.assertLess(receive.index("handle_remote_service_frame(command)"),
                        receive.index("if (length >= recovery_capacity)"))
        self.assertLess(receive.index("if (length >= recovery_capacity)"),
                        receive.index("_enqueue(serial_line_buffer"))
        arc = (ROOT /
               "lib/Marlin/Marlin/src/gcode/motion/G2_G3.cpp").read_text()
        self.assertIn("Extrusion rejected: no tool selected", arc)

    def test_actual_full_fifo_rejection_restores_accepted_line(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        receive = function(source, "void GCodeQueue::get_serial_commands()")
        overflow = function(receive, "if (length >= recovery_capacity)")
        code = r'''
#include <cassert>
int last_N, accepted_line_before, length, errors=0, requests=0, resend=0;
bool npos=true;
constexpr int recovery_capacity=10;
#define SERIAL_ECHOPAIR(label, number) (++requests, resend=(number))
#define SERIAL_EOL() ((void)0)
#define SERIAL_ERROR_MSG(...) (++errors)
int main() {
 for(int line=0;line<1000;++line) {
  accepted_line_before=line; last_N=line+1; length=10;
  for(int once=0;once<1;++once) {
'''
        end = r'''
   assert(false); // Overflow must not reach enqueue.
  }
  assert(last_N==line && resend==line+1);
 }
 assert(requests==1000 && errors==0);
 npos=false;
 for(int once=0;once<1;++once) {
'''
        finish = r'''
  assert(false);
 }
 assert(errors==1 && requests==1000);
}
'''
        self.run_cpp(code + overflow + end + overflow + finish)


if __name__ == "__main__":
    unittest.main()

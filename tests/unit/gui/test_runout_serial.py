"""Execute the production sensor dispatcher with serial/crash/toolchange states."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class SerialRunout(unittest.TestCase):

    def test_actual_dispatcher_rechecks_empty_tool_eight(self):
        source = (ROOT /
                  'src/feature/filament_sensor/filament_sensors_handler.cpp'
                  ).read_text()
        handler = 'void FilamentSensors::process_events()' + source.split(
            'void FilamentSensors::process_events()', 1)[1].split(
                'void FilamentSensors::process_enable_state_update()', 1)[0]
        stub = r'''
#include <cassert>
#include <optional>
#define HAS_INDX() 1
#define HAS_CRASH_DETECTION() 1
#define HAS_SERIAL_PRINT() 1
#define log_info(...) ((void)0)
enum class FilamentSensorState { NoFilament, HasFilament, Disabled, NotConnected };
enum class LogicalFilamentSensor { primary_runout, extruder, side, closest_to_nozzle };
enum class ToolchangePhase { none, moving };
struct PrusaToolChanger { using ToolchangePhase = ::ToolchangePhase; };
struct Sensor { enum class Event { no_event, filament_removed, filament_inserted };
 FilamentSensorState state = FilamentSensorState::NoFilament;
 Event last_event() { return Event::no_event; }
 auto get_state() { return state; }
} side;
struct IFSensor : Sensor {};
struct Crash_s { enum State { IDLE, PRINTING, RECOVERY }; State state = IDLE;
 auto get_state() { return state; }
} crash_s;
struct { ToolchangePhase value = ToolchangePhase::none; auto phase() { return value; } } prusa_toolchanger;
namespace marlin_server {
 enum class State { Printing, Preparing, Paused, Idle };
 bool serial = true, printing = true, paused = false; int injections = 0;
 bool serial_print_active() { return serial; }
 bool is_printing() { return printing; }
 bool printer_paused_extended() { return paused; }
 bool is_processing() { return false; }
 template<class T> void gcode_interrupt(T) { ++injections; }
 template<class T> void inject(T) {}
}
struct { marlin_server::State print_state = marlin_server::State::Printing; } vars;
auto &marlin_vars() { return vars; }
struct GCodeLiteral { const char *gcode; float parameter = 0; };
struct Flag { bool get() { return false; } };
struct { Flag fs_autoload_enabled; template<class A, class B> void set_filament_type(A,B) {} } store;
auto &config_store() { return store; }
using VirtualToolIndex = int;
struct PhysicalToolIndex { static constexpr unsigned count = 8;
 static PhysicalToolIndex from_raw(unsigned) { return {}; }
 std::optional<int> currently_selected_virtual_tool() const { return 7; }
};
namespace stdext { template<class T> auto get_optional(std::optional<T> v) { return v; } }
enum class FilamentType { none };
#define Z_AXIS_LOAD_POS 40
struct FilamentSensors {
 bool locked = false, m600_sent = false, has_mmu = false, autoload_sent = false, extruder_fs_independent = true;
 unsigned tool_index = 7;
 bool isEvLocked() { return locked; }
 bool isAutoloadLocked() { return false; }
 Sensor *sensor(LogicalFilamentSensor s) { return s == LogicalFilamentSensor::extruder ? nullptr : &side; }
 bool no_filament_surely(LogicalFilamentSensor) { return false; }
 bool has_filament_surely(LogicalFilamentSensor) { return false; }
 void process_events();
};
'''
        main = r'''
int main() {
 FilamentSensors f;
 auto tick = [&] { f.process_events(); };
 f.locked = true; tick(); assert(!marlin_server::injections);
 f.locked = false;
 prusa_toolchanger.value = ToolchangePhase::moving; tick(); assert(!marlin_server::injections);
 prusa_toolchanger.value = ToolchangePhase::none;
 crash_s.state = Crash_s::RECOVERY; tick(); assert(!marlin_server::injections);
 crash_s.state = Crash_s::IDLE;
 marlin_server::serial = false; tick(); assert(!marlin_server::injections);
 marlin_server::serial = true;
 vars.print_state = marlin_server::State::Preparing; tick(); assert(!marlin_server::injections);
 vars.print_state = marlin_server::State::Printing;
 side.state = FilamentSensorState::NotConnected; tick(); assert(!marlin_server::injections);
 side.state = FilamentSensorState::Disabled; tick(); assert(!marlin_server::injections);
 side.state = FilamentSensorState::NoFilament;
 marlin_server::paused = true; tick(); assert(!marlin_server::injections);
 marlin_server::paused = false;
 tick(); assert(marlin_server::injections == 1);
 tick(); assert(marlin_server::injections == 1);
 f.m600_sent = false; side.state = FilamentSensorState::HasFilament; tick(); assert(marlin_server::injections == 1);
 side.state = FilamentSensorState::NoFilament;
 crash_s.state = Crash_s::PRINTING; tick(); assert(marlin_server::injections == 2);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'runout.cpp'
            cpp.write_text(stub + handler + main)
            binary = Path(directory) / 'runout'
            subprocess.run(['c++', '-std=c++23',
                            str(cpp), '-o',
                            str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True)

    def test_manual_jog_cannot_be_starved_before_first_segment(self):
        source = (ROOT / 'src/marlin_stubs/G123.cpp').read_text()
        self.assertIn('if (segment_planned)', source)
        self.assertIn('!jog_telemetry(queue.command_buffer[index])', source)
        self.assertLess(source.index('line_to_current_position('),
                        source.index('segment_planned = true;'))
        self.assertIn('axis_unhomed_error(required_axes', source)
        self.assertIn('Manual move blocked by dock keep-out', source)
        self.assertIn('thermalManager.tooColdToExtrude(active_extruder)',
                      source)
        self.assertIn(
            '!PhysicalToolIndex::currently_selected_opt().has_value()', source)

    def test_jog_telemetry_classification_keeps_controls_interruptible(self):
        code = r'''
#include <manual_motion_safety.hpp>
#include <cassert>
int main() {
 using buddy::manual_motion_safety::jog_telemetry;
 for (const char *s : {"M117 clock", "N37 M117 clock*82", "M105", "M114", "M73 P0"}) assert(jog_telemetry(s));
 for (const char *s : {"G123 E5", "M104 S0", "M112", "M108", "M1170", "M117.1", "@RME FILAMENT SET", "G1 E5"}) assert(!jog_telemetry(s));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'jog.cpp'
            cpp.write_text(code)
            binary = Path(directory) / 'jog'
            subprocess.run([
                'c++', '-std=c++23', '-I',
                str(ROOT / 'src/common'),
                str(cpp), '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

    def test_actual_jog_loop_completes_hot_e_move_with_queued_display_updates(
            self):
        source = (ROOT / 'src/marlin_stubs/G123.cpp').read_text()
        loop = 'bool segment_planned = false;' + source.split(
            'bool segment_planned = false;', 1)[1].split('\n}\n', 1)[0]
        stub = r'''
#include <manual_motion_safety.hpp>
#include <array>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdint>
using namespace buddy::manual_motion_safety;
struct xyze_float_t {
 std::array<float,4> a;
 float &operator[](unsigned i) { return a[i]; }
 float operator[](unsigned i) const { return a[i]; }
 bool operator==(const xyze_float_t &) const = default;
 xyze_float_t operator-(const xyze_float_t &b) const { auto r=*this; for(unsigned i=0;i<4;++i)r.a[i]-=b[i]; return r; }
 xyze_float_t operator*(float b) const { auto r=*this; for(auto &v:r.a)v*=b; return r; }
 xyze_float_t &operator+=(const xyze_float_t &b) { for(unsigned i=0;i<4;++i)a[i]+=b[i]; return *this; }
 float magnitude() const {float r=0;for(float v:a)r+=v*v;return std::sqrt(r);}
};
xyze_float_t operator*(xyze_float_t a, xyze_float_t b) { for(unsigned i=0;i<4;++i)a[i]*=b[i];return a; }
struct GCodeQueue { static constexpr unsigned recovery_capacity=8; unsigned length=2,index_r=0; char command_buffer[8][32]{}; } queue;
struct { bool is_full() { return false; } } planner;
void idle(bool) {}
xyze_float_t current_position{{{7,8,9,0}}}, target_pos{{{7,8,9,3}}};
const xyze_float_t feedrate{{{1,1,1,1}}}, feedrate_inv{{{1,1,1,1}}};
constexpr float max_segment_duration_s = .02f;
unsigned segments=0;
void line_to_current_position(float) { assert(++segments < 1000); }
void run() {
'''
        main = r'''
}
int main() {
 std::strcpy(queue.command_buffer[1], "N37 M117 clock*42");
 run(); assert(std::abs(current_position[3]-3)<.011f); assert(segments>100);
 assert(current_position[0]==7 && current_position[1]==8 && current_position[2]==9);
 current_position[3]=0; segments=0;
 std::strcpy(queue.command_buffer[1], "G123 E1");
 run(); assert(segments==1 && current_position[3]<.1f);
 current_position[3]=0; segments=0; queue.length=3;
 std::strcpy(queue.command_buffer[1], "M117 clock");
 std::strcpy(queue.command_buffer[2], "G123 E1");
 run(); assert(segments==1 && current_position[3]<.1f);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'jog.cpp'
            cpp.write_text(stub + loop + main)
            binary = Path(directory) / 'jog'
            subprocess.run([
                'c++', '-std=c++23', '-I',
                str(ROOT / 'src/common'),
                str(cpp), '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

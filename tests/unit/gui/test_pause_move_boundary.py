"""Execute real line/arc generators and pause/prime state handlers on a host.

The planner mock drains between segments and reenters the firmware loop at
every selected boundary. No printer motion or host connection is performed.
"""
import unittest

import test_serial_pause_recovery as fixture


class PauseMoveBoundary(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_failed_pickup_does_not_advance_resume_or_override_abort(self):
        source = (fixture.ROOT / "src/common/marlin_server.cpp").read_text()
        begin = fixture.function(source, "void resuming_begin(void)")
        pickup = fixture.function(begin, "if (server.resume.active_tool !=")
        self.run_cpp(r'''
#include <cassert>
#include <marlin_server_types/marlin_server_state.h>
using marlin_server::State;
struct { State print_state=State::Resuming_Begin; struct {int active_tool=0;} resume; } server;
struct PrusaToolChanger { static constexpr int MARLIN_NO_TOOL_PICKED=8; };
struct PhysicalToolIndex { static int from_raw(int i) {return i;} };
enum class tool_return_t {no_return};
int outcome=0, continued=0, parked=0;
bool tool_change(int,tool_return_t) {
 if(outcome==2) server.print_state=State::Aborting_Begin;
 return outcome==1;
}
struct { bool did_trigger() {return false;} } crash_s;
namespace mapi {
enum class ParkPosition {park};
struct Position {Position without_z_move() {return *this;}};
Position get_parking_position(ParkPosition) {return {};}
void park(Position) {++parked;}
}
void pick() {
''' + pickup + r'''
 ++continued;
}
int main() {
 pick(); assert(server.print_state==State::Paused && continued==0 && parked==0);
 server.print_state=State::Resuming_Begin; outcome=2; pick();
 assert(server.print_state==State::Aborting_Begin && continued==0 && parked==0);
 server.print_state=State::Resuming_Begin; outcome=1; pick();
 assert(continued==1 && parked==1);
}
''')

    def test_complete_move_then_dock_and_prime_before_return(self):
        root = fixture.ROOT
        server = (root / "src/common/marlin_server.cpp").read_text()
        start = server.index("    case State::Pausing_WaitIdle:\n",
                             server.index("case State::Pausing_Begin:"))
        pause = server[start:server.index("    case State::Paused:", start)]
        start = server.index(
            "    case State::Resuming_ExecutingGCodeInterrupt: {")
        resume = server[start:server.
                        index("    case State::Aborting_Begin:", start)]
        prime = fixture.function(server, "bool unpark_prime()")
        line = fixture.function(
            (root / "lib/Marlin/Marlin/src/module/motion.cpp").read_text(),
            "void prepare_move_to(")
        arc = fixture.function(
            (root /
             "lib/Marlin/Marlin/src/gcode/motion/G2_G3.cpp").read_text(),
            "void plan_arc(")
        preamble = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include <marlin_server_types/marlin_server_state.h>
using marlin_server::State;
#define HAS_INDX() 1
namespace buddy::serial_collision { void suspend() {} bool pending() { return false; } }
#define HAS_CRASH_DETECTION() 1
#define HAS_FILAMENT_TRACKER() 1
#define HAS_AUTO_RETRACT() 1
#define HAS_NOZZLE_CLEANER() 1
#define HAS_EMERGENCY_STOP() 0
#define HAS_SERIAL_PRINT() 1
#define EITHER(...) 0
#define ENABLED(x) x
#define AUTO_BED_LEVELING_UBL 1
#define CNC_WORKSPACE_PLANES 0
#define FEEDRATE_SCALING 0
#define OPTARG(c,...) , __VA_ARGS__
#define ARC_LIJKUVWE_CODE(L,I,J,K,U,V,W,E) L; E
#define GANG_N(n,L,...) L
#define TERN0(...) false
#define HAS_EXTRUDERS 1
#define N_ARC_CORRECTION 25
#define MAX_ARC_DEVIATION 0.01f
#define MIN_ARC_SEGMENTS_PER_SEC 10
#define MIN_ARC_SEGMENT_MM 0.1f
#define MAX_ARC_SEGMENT_MM 1.0f
#define LEVELED_SEGMENT_LENGTH 1.0f
#define RADIANS(x) ((x)*0.017453292519943295f)
#define HYPOT std::hypot
#define ATAN2 std::atan2
#define ABS std::abs
#define NEAR(a,b) (std::abs((a)-(b))<1e-6f)
#define NEAR_ZERO(a) NEAR(a,0)
#define MMS_SCALED(x) (x)
#define ELAPSED(a,b) ((a)>=(b))
#define release_assert assert
#define debug_assert assert
using feedRate_t=float;
using millis_t=uint32_t;
enum AxisEnum {X_AXIS,Y_AXIS,Z_AXIS,E_AXIS};
struct xy_float_t {
 union { float data[2]; struct { float x,y; }; struct { float a,b; }; };
 float magnitude() const { return std::hypot(x,y); }
 float operator[](int n) const { return data[n]; }
 xy_float_t operator-() const { return {{{-x,-y}}}; }
};
struct xyze_pos_t {
 union { float data[4]; struct { float x,y,z,e; }; };
 float &operator[](int n) { return data[n]; }
 float operator[](int n) const { return data[n]; }
 xy_float_t xy() const { return {{{x,y}}}; }
 xyze_pos_t operator-(const xyze_pos_t &b) const { return {{{x-b.x,y-b.y,z-b.z,e-b.e}}}; }
 xyze_pos_t operator/(float d) const { return {{{x/d,y/d,z/d,e/d}}}; }
 xyze_pos_t &operator+=(const xyze_pos_t &b) { for(int i=0;i<4;++i) data[i]+=b[i]; return *this; }
};
using xyze_float_t=xyze_pos_t;
struct NoTool {};
struct PrusaToolChanger { static constexpr int MARLIN_NO_TOOL_PICKED=255; };
bool selected=true, idle_running=false, queued=false, cold=false, homed=true;
bool fail_clean=false, cancel_clean=false, abort_restore=false, cold_during_clean=false;
int active_extruder=0, docked=0, primed=0, returned=0, synced=0, host_resumes=0, ze_returns=0;
int feedrate_percentage=100;
bool abort_resuming=false;
float physical_retraction=0, saved_retraction=0;
int pause_at=0;
std::vector<xyze_pos_t> path;
xyze_pos_t current_position{};
float feedrate_mm_s=30;
struct PhysicalToolIndex {
 static std::variant<int,NoTool> currently_selected() { return selected ? std::variant<int,NoTool>(0) : NoTool{}; }
 static std::optional<int> currently_selected_opt() { return selected ? std::optional<int>(0) : std::nullopt; }
};
struct {
 bool print_is_serial=true;
 State print_state=State::Printing;
 struct { xyze_pos_t pos{}; int fan_speed=50, print_speed=100, active_tool=0; } resume;
} server;
struct { std::optional<float> pause_retracted_distance; bool notify_serial_host_on_resume=true; struct { bool is_empty() { return true; } } gcode_interrupt_command; } print_state;
struct MoveHints { bool extrusion_safety_checks:1=false, is_printing_move:1=true; };
struct PrepareMoveHints { bool apply_motion_limits=false, scale_feedrate=false, do_segment=true; MoveHints move; };
struct PlannerHints { MoveHints move; };
void firmware_loop();
void nested_idle() {
 bool old=idle_running; idle_running=true; firmware_loop(); idle_running=old;
}
void accept(xyze_pos_t p) {
 assert(selected); path.push_back(p);
 if(int(path.size())==pause_at) server.print_state=State::Pausing_WaitIdle;
 nested_idle();
 assert(selected && docked==0); // Never dock in the middle of the G-code.
}
struct {
 bool processing_=false, draining_=false, leveling_active=true;
 float max_printed_z=0;
 bool processing() { return processing_; }
 void synchronize() { nested_idle(); }
 bool draining() { return draining_; }
 bool leveling_active_at_z(float) { return true; }
 void buffer_segment(xyze_pos_t p,float,std::variant<int,NoTool>,PlannerHints) { accept(p); }
 bool buffer_line(xyze_pos_t p,float,std::variant<int,NoTool>,PlannerHints) { accept(p); return true; }
} planner;
bool is_processing() { return planner.processing_ || queued; }
bool all_axes_homed() { return homed; }
void apply_motion_limits(xyze_pos_t&) {}
bool position_is_reachable(xy_float_t) { return true; }
xyze_pos_t to_machine_pos(xyze_pos_t p) { return p; }
void set_current_position(xyze_pos_t p) { current_position=p; }
millis_t millis() { static millis_t now=0; return now+=201; }
float fast_sqrt(float x) { return std::sqrt(x); }
void idle(bool) { nested_idle(); }
struct { void manage_heater() {} bool tooColdToExtrude(int) { return cold; } void set_print_fan_speed(int) {} } thermalManager;
struct { void G0_G1() { assert(false); } } gcode;
struct Crash_s { enum { IDLE,PRINTING,RECOVERY,REPEAT_WAIT,REPLAY }; int state=IDLE;
 int get_state() { return state; } void set_state(int s) { state=s; } bool is_toolchange_event() { return false; } } crash_s;
struct { void enable_globally(bool) {} } endstops;
struct { bool isPaused() { return true; } void start() {} } print_job_timer;
namespace buddy {
struct Auto { std::optional<float> retracted_distance(int) { return std::nullopt; } } autoretract;
Auto &auto_retract() { return autoretract; }
struct Tracker { std::optional<float> get_retracted_distance(int) { return physical_retraction; } } tracker;
Tracker &filament_tracker() { return tracker; }
namespace standard_feedrates { enum class Extruder {deretract}; float current_extruder(Extruder) { return 10; } }
}
namespace mapi {
struct ParkPosition { float x,y; };
void park(ParkPosition p) { current_position.x=p.x; current_position.y=p.y; }
void restore_retracted_distance(float distance,float) {
 current_position.e+=physical_retraction-distance;
 physical_retraction=distance;
 nested_idle();
 if(abort_restore) planner.draining_=true;
}
}
void retract_and_lift() { current_position.e-=3; physical_retraction+=3; }
void park_head(bool) { current_position.x=250; current_position.y=160; }
enum class tool_return_t {no_return};
bool tool_change(NoTool,tool_return_t) {
 assert(++docked==1); nested_idle(); selected=false; return true;
}
namespace SerialPrinting {
void paused(const char*) {} void notify_error(const char*,const char*,const char*) {}
void notify_workflow(const char*,const char*,const char*,int) {}
void resume(bool) {
 assert(!idle_running && primed==1 && returned==1 && ze_returns==1);
 ++host_resumes;
}
void resumed() {}
}
namespace nozzle_cleaner {
enum class Sequence {purge_clean};
bool load_and_execute(Sequence) {
 ++primed; nested_idle();
 if(cold_during_clean) cold=true;
 if(cancel_clean) server.print_state=State::Aborting_Begin;
 if(fail_clean) return false;
 current_position.e+=17; physical_retraction=8; // Purge then auto-retract.
 return true;
}
}
void sync_e_position_to(float e) { ++synced; current_position.e=e; }
void sync_plan_position_e() {}
constexpr float X_BED_SIZE=250, Y_BED_SIZE=250;
bool unpark_prime();
void unpark_head_XY() { assert(synced==1); ++returned; current_position.x=server.resume.pos.x; current_position.y=server.resume.pos.y; }
void unpark_head_ZE() { assert(++ze_returns==1); nested_idle(); assert(host_resumes==0); }
void plan_arc(const xyze_pos_t&,const xy_float_t&,bool,uint8_t,bool=true);
'''
        main = r'''
void reset() {
 server.print_state=State::Printing; selected=true; idle_running=false;
 planner.processing_=planner.draining_=false;
 docked=primed=returned=synced=host_resumes=ze_returns=0; path.clear();
 queued=cold=fail_clean=cancel_clean=abort_restore=cold_during_clean=false; homed=true;
 physical_retraction=saved_retraction; current_position={{{10,0,0.2f,100}}};
 print_state.pause_retracted_distance.reset(); crash_s.state=Crash_s::IDLE;
}
int main() {
 // G1, G2, G3, full circles and helical arcs. Pause at every planned segment,
 // including an empty planner while still inside the generating function.
 for(int shape=0;shape<5;++shape) {
  reset(); pause_at=0;
  xyze_pos_t target=shape==3 ? xyze_pos_t{{{10,0,0.2f,110}}} : xyze_pos_t{{{0,10,shape==4?2.2f:0.2f,110}}};
  if(shape==0) prepare_move_to(target,30,{});
  else plan_arc(target,xy_float_t{{{-10,0}}},shape==1,0);
  const auto reference=path;
  assert(reference.size()>5);
  for(size_t boundary=1;boundary<=reference.size();++boundary) {
   for(float retracted : {0.f,0.7f,3.f}) {
    saved_retraction=retracted; reset(); pause_at=boundary;
    if(shape==0) prepare_move_to(target,30,{});
    else plan_arc(target,xy_float_t{{{-10,0}}},shape==1,0);
    assert(path.size()==reference.size() && docked==0);
    for(size_t i=0;i<path.size();++i) for(int axis=0;axis<4;++axis)
      assert(std::abs(path[i][axis]-reference[i][axis])<1e-5f);
    planner.processing_=true; firmware_loop(); assert(docked==0);
    planner.processing_=false; firmware_loop(); firmware_loop();
    assert(docked==1 && !selected && server.print_state==State::Paused);
    assert(server.resume.pos.e==target.e);
    assert(print_state.pause_retracted_distance==saved_retraction);
    // Ordinary serial resume: crash state deliberately IDLE, not PRINTING.
    selected=true; server.print_state=State::Resuming_ExecutingGCodeInterrupt;
    firmware_loop();
    assert(primed==1 && returned==1 && synced==1);
    assert(physical_retraction==saved_retraction && current_position.e==target.e);
    firmware_loop(); firmware_loop();
    assert(server.print_state==State::Printing && host_resumes==1);
   }
  }
 }
 // Failed/cancelled priming must not reset E or return to the print.
 for(int failure=0;failure<7;++failure) {
  reset(); server.print_state=State::Resuming_ExecutingGCodeInterrupt;
  print_state.pause_retracted_distance=0;
  selected=failure!=0; cold=failure==1; homed=failure!=2;
  fail_clean=failure==3; cancel_clean=failure==4; abort_restore=failure==5;
  cold_during_clean=failure==6;
  firmware_loop();
  assert(returned==0 && synced==0);
  assert(server.print_state==(failure==4?State::Aborting_Begin:State::Paused));
 }
 // SD printing retains its ordinary PRINTING-state prime behavior.
 reset(); server.print_is_serial=false; crash_s.state=Crash_s::PRINTING;
 server.print_state=State::Resuming_ExecutingGCodeInterrupt;
 firmware_loop(); assert(primed==1 && returned==1);
 // Host startup/control pause before the first T command: no purge, no E sync.
 reset(); selected=false; homed=false; cold=true;
 server.print_is_serial=true; server.resume.active_tool=255;
 assert(unpark_prime()); assert(primed==0 && synced==0);
 server.resume.active_tool=0; assert(!unpark_prime());
 server.resume.active_tool=255; server.print_is_serial=false; assert(!unpark_prime());
}
'''
        self.run_cpp(
            preamble + line + arc + prime +
            "\nvoid firmware_loop() { switch(server.print_state) {\n" + pause +
            resume + "default: break;\n} }\n" + main)


if __name__ == "__main__":
    unittest.main()

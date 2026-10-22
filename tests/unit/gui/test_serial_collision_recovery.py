"""Fault-inject the actual bounded journal and serial recovery dispatcher."""
import ast
import unittest

import test_serial_pause_recovery as fixture


class SerialCollisionRecovery(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_inspection_is_required_before_motion_or_host_resume(self):
        source = (fixture.ROOT / "src/common/marlin_server.cpp").read_text()
        handler = source[
            source.index("    case State::CrashRecovery_Repeated_Crash: {"):]
        gate = fixture.function(
            handler,
            "if (server.print_is_serial && buddy::serial_collision::pending())"
        )
        resume = fixture.function(
            source, "void print_resume(bool notify_serial_host)")
        resume_gate = fixture.function(
            resume,
            "if (server.print_is_serial && buddy::serial_collision::pending()")
        self.run_cpp(r'''
#include <cassert>
#include <marlin_server_types/marlin_server_state.h>
using namespace marlin_server;
enum class Response { _none, Continue, Abort };
enum class PhasesCrashRecovery { serial_collision, home };
enum class AxisHomeLevel { imprecise };
constexpr int Z_AXIS=2;
constexpr float Z_MAX_POS=250;
bool pending=true, unchanged=true, known=true;
int retracts=0, homes=0, resume_calls=0, warnings=0;
Response response=Response::_none;
struct { bool print_is_serial=true; State print_state=State::CrashRecovery_Repeated_Crash; } server;
struct { float z=10; } current_position;
struct { bool is_homed(int,AxisHomeLevel) {return known;} } axes_home_level;
namespace buddy::serial_collision {
bool pending() {return ::pending;}
bool tool_unchanged() {return unchanged;}
}
namespace marlin_server { Response get_response_from_phase(PhasesCrashRecovery) {return response;} }
namespace SerialPrinting { void notify_error(const char*,const char*,const char*) {++warnings;} }
void retract() {++retracts;}
void fsm_change(PhasesCrashRecovery,int) {++homes;}
void inspect() {
 switch(server.print_state) {
 case State::CrashRecovery_Repeated_Crash:
''' + gate + r'''
 default: break;
 }
}
void host_resume() {
''' + resume_gate + r'''
 ++resume_calls;
}
int main() {
 inspect(); host_resume();
 assert(retracts==0 && homes==0 && resume_calls==0);
 assert(server.print_state==State::CrashRecovery_Repeated_Crash);
 response=Response::Continue; inspect();
 assert(retracts==1 && homes==1 && server.print_state==State::CrashRecovery_Retracting);
 for(int fault=0;fault<4;++fault) {
  server.print_state=State::CrashRecovery_Repeated_Crash;
  known=fault!=0; unchanged=fault!=1; current_position.z=fault==2 ? 248 : 10;
  response=fault==3 ? Response::Abort : Response::Continue;
  inspect(); assert(server.print_state==State::Aborting_Begin);
  assert(retracts==1 && homes==1);
 }
 // The native post-home Paused transition can resume; a raw host Resume
 // cannot impersonate the dialog's Continue while inspection is pending.
 server.print_state=State::Paused; host_resume(); assert(resume_calls==1);
}
''')

    def test_retirement_uses_physical_planner_tail_and_freezes_on_isr(self):
        source = (fixture.ROOT /
                  "lib/Marlin/Marlin/src/feature/prusa/crash_recovery.hpp"
                  ).read_text()
        method = fixture.function(
            source, "bool serial_checkpoint_in_flight(uint32_t token) const")
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <serial_collision_journal.hpp>
#define BLOCK_BUFFER_SIZE 32
namespace buddy { struct InterruptDisabler {}; }
struct { uint8_t block_buffer_tail=30,block_buffer_head=2; } planner;
struct Crash {
 enum {PRINTING,TRIGGERED_ISR}; int state=PRINTING;
 struct Block {uint32_t sdpos=~0u;} crash_block[32];
''' + method + r'''
};
int main() {
 Crash crash;
 crash.crash_block[30].sdpos=100; crash.crash_block[31].sdpos=101;
 crash.crash_block[0].sdpos=102; crash.crash_block[1].sdpos=103;
 for(unsigned id=100;id<=103;++id) assert(crash.serial_checkpoint_in_flight(id));
 assert(!crash.serial_checkpoint_in_flight(99));
 // Physical completion moves tail. Lookahead's earlier/nonbusy retirement
 // must not release a checkpoint; this scan never consults nonbusy/planned.
 planner.block_buffer_tail=31;
 assert(!crash.serial_checkpoint_in_flight(100));
 assert(crash.serial_checkpoint_in_flight(101));
 crash.state=Crash::TRIGGERED_ISR;
 assert(crash.serial_checkpoint_in_flight(99));
 buddy::serial_collision::Journal<96> journal;
 auto first=journal.append("G1 X1",20,8)->token;
 auto second=journal.append("G1 X2",20,8)->token;
 journal.retire_completed([&](uint32_t t){return t==second;});
 assert(!journal.freeze(first)); // completed prefix really retired
 journal.clear();
 first=journal.append("G1 X1",20,8)->token;
 second=journal.append("G1 X2",20,8)->token;
 assert(journal.freeze(first));
 journal.retire_completed([](uint32_t){return false;});
 assert(journal.next()->token==first); // no retirement after a crash
}
''')

    def test_native_partial_replay_preserves_line_arc_and_extrusion_suffix(
            self):
        # Reuse the existing motion-generator fixture, not a second rewritten
        # arc algorithm. Add the actual planner's partial-replay decision.
        tree = ast.parse(
            (fixture.ROOT /
             "tests/unit/gui/test_pause_move_boundary.py").read_text())
        preamble = next(
            ast.literal_eval(node.value) for node in ast.walk(tree)
            if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "preamble"
                for t in node.targets))
        preamble = preamble.replace(
            "void accept(xyze_pos_t p) {",
            "bool replay_skip();\nvoid accept(xyze_pos_t p) {")
        preamble = preamble.replace("{ accept(p); }",
                                    "{ if(!replay_skip()) accept(p); }")
        preamble = preamble.replace(
            "{ accept(p); return true; }",
            "{ if(!replay_skip()) accept(p); return true; }")
        preamble = preamble.replace(
            "bool is_toolchange_event() { return false; } } crash_s;",
            "bool is_toolchange_event() { return false; } int segments_finished=0; xyze_pos_t crash_machine_position{}; } crash_s;"
        )
        planner = (fixture.ROOT /
                   "lib/Marlin/Marlin/src/module/planner.cpp").read_text()
        replay = fixture.function(
            planner, "if (crash_s.get_state() == Crash_s::REPLAY)")
        line = fixture.function(
            (fixture.ROOT /
             "lib/Marlin/Marlin/src/module/motion.cpp").read_text(),
            "void prepare_move_to(")
        arc = fixture.function(
            (fixture.ROOT /
             "lib/Marlin/Marlin/src/gcode/motion/G2_G3.cpp").read_text(),
            "void plan_arc(")
        self.run_cpp(preamble + r'''
void firmware_loop() {}
xyze_pos_t restart{};
void set_machine_position_mm(xyze_pos_t p) { restart=p; }
bool replay_skip() {
''' + replay + r'''
 return false;
}
''' + line + arc + r'''
int main() {
 for(int shape=0;shape<5;++shape) {
  const xyze_pos_t start{{{10,0,.2f,100}}};
  const xyze_pos_t target = shape==3 ? xyze_pos_t{{{10,0,.2f,110}}} : xyze_pos_t{{{0,10,shape==4?2.2f:.2f,110}}};
  current_position=start; path.clear(); crash_s.state=Crash_s::PRINTING;
  if(shape==0) prepare_move_to(target,30,{});
  else plan_arc(target,xy_float_t{{{-10,0}}},shape==1,0);
  const auto reference=path; assert(reference.size()>5);
  for(size_t completed=0;completed<reference.size();++completed) {
   for(float fraction : {.0f,.25f,.5f,.99f}) {
    auto partial = completed ? reference[completed-1] : start;
    for(int axis=0;axis<4;++axis) partial[axis] += fraction*(reference[completed][axis]-partial[axis]);
    crash_s.segments_finished=completed; crash_s.crash_machine_position=partial;
    crash_s.state=Crash_s::REPLAY; current_position=start; path.clear(); restart=start;
    if(shape==0) prepare_move_to(target,30,{});
    else plan_arc(target,xy_float_t{{{-10,0}}},shape==1,0);
    assert(path.size()==reference.size()-completed && crash_s.state==Crash_s::PRINTING);
    for(int axis=0;axis<4;++axis) assert(std::abs(restart[axis]-partial[axis])<1e-5f);
    for(size_t i=0;i<path.size();++i) for(int axis=0;axis<4;++axis)
     assert(std::abs(path[i][axis]-reference[completed+i][axis])<1e-5f);
    // The physical remainder plus already-executed E equals the original E.
    float total=partial.e-start.e, previous=restart.e;
    for(auto point:path) { total+=point.e-previous; previous=point.e; }
    assert(std::abs(total-(target.e-start.e))<1e-4f);
   }
  }
 }
}
''')

    def test_dispatcher_barriers_checkpoints_and_internal_replay(self):
        source = (fixture.ROOT / "src/common/serial_printing.cpp").read_text()
        start = source.index("namespace buddy::serial_collision {")
        end = source.index("} // namespace buddy::serial_collision", start)
        runtime = source[start:end] + "}\n"
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <serial_collision_journal.hpp>
#include <rme_passive_command.hpp>
#define MAX_CMD_SIZE 96
enum AxisEnum {Z_AXIS};
enum class AxisHomeLevel { imprecise };
struct Home { bool known=true; bool is_homed(AxisEnum,AxisHomeLevel) const { return known; } };
struct PhysicalToolIndex {
    int i;
    bool operator==(const PhysicalToolIndex&) const=default;
    static std::optional<PhysicalToolIndex> currently_selected_opt();
};
std::optional<PhysicalToolIndex> picked=PhysicalToolIndex{1};
auto PhysicalToolIndex::currently_selected_opt()->std::optional<PhysicalToolIndex> { return picked; }
using VirtualToolIndex=PhysicalToolIndex;
namespace marlin_server {
enum class State { Printing, Paused, Aborted };
bool serial=true;
bool serial_print_active() { return serial; }
}
struct { marlin_server::State print_state=marlin_server::State::Printing; } vars;
auto &marlin_vars() { return vars; }
bool live=true;
namespace phase_stepping {
bool enabled=false;
bool any_axis_enabled() {return enabled;}
}
struct Crash_s {
    enum State { IDLE, PRINTING, TRIGGERED_ISR, RECOVERY, REPLAY };
    static constexpr int RECOVER_PARTIAL_REPLAY=8;
    State state=IDLE;
    bool active=false, toolchange=false, enabled=true;
    bool has_motion_snapshot=true;
    uint32_t sdpos=~0u;
    int recover_flags=RECOVER_PARTIAL_REPLAY;
    Home crash_axes_home_level;
    State get_state() const { return state; }
    void set_state(State s) { state=s; active=true; }
    void activate() { active=true; }
    void deactivate() { active=false; }
    bool is_toolchange_event() const { return toolchange; }
    bool serial_checkpoint_in_flight(uint32_t) const { return live; }
    bool is_enabled() const {return enabled;}
    bool is_active() const {return active;}
} crash_s;
float feedrate_mm_s=25;
bool crash_on_drain=false, pause_on_drain=false, crash_on_replay=false;
int drains=0;
int waits=0;
void idle(bool) {
    ++waits;
    if(crash_on_drain) crash_s.state=Crash_s::TRIGGERED_ISR;
    else live=false;
}
struct { void synchronize() {
    ++drains;
    if(crash_on_drain) crash_s.state=Crash_s::TRIGGERED_ISR;
    if(pause_on_drain) vars.print_state=marlin_server::State::Paused;
} } planner;
std::vector<std::string> replayed;
std::vector<float> feeds;
std::vector<int> modes;
struct {
    uint8_t axis_relative=8;
    void process_subcommands_now(char *command) {
        replayed.emplace_back(command); feeds.push_back(feedrate_mm_s);
        modes.push_back(axis_relative);
        crash_s.state=crash_on_replay ? Crash_s::TRIGGERED_ISR : Crash_s::PRINTING;
    }
} gcode;
''' + runtime + r'''
using namespace buddy::serial_collision;
void fresh() {
    reset(); crash_s={}; drains=waits=0; live=true; replayed.clear(); feeds.clear(); modes.clear();
    vars.print_state=marlin_server::State::Printing; marlin_server::serial=true;
    picked=PhysicalToolIndex{1};
    phase_stepping::enabled=false;
    crash_on_drain=pause_on_drain=crash_on_replay=false;
}
int main() {
    fresh();
    const char *motion[] = {"G0 X1", "N20 G1 X2 E.2*3", "G2 X3 I1 J1", "G3 X4 I1 J1",
        "G01 X5", "G00 X6", "G02 X7 I1 J1", "G03 X8 I1 J1"};
    uint32_t tokens[8];
    for(int i=0;i<8;++i) {
        feedrate_mm_s=20+i; gcode.axis_relative=i;
        assert(prepare(motion[i])); tokens[i]=gcode_position(~0u);
        assert(tokens[i]!=~0u && (!i || tokens[i]!=tokens[i-1]));
    }
    assert(drains==0 && crash_s.active);
    // A full journal waits for retirement; a collision during that wait must
    // retain all live checkpoints without consuming the next host command.
    crash_s.sdpos=tokens[2]; crash_on_drain=true;
    assert(!prepare("G1 X9") && drains==0 && waits==1);
    assert(begin() && pending());
    vars.print_state=marlin_server::State::Paused;
    assert(!replay_one() && replayed.empty()); // confirmation/home/prime are not bypassed
    assert(tool_unchanged()); picked=PhysicalToolIndex{2}; assert(!tool_unchanged());
    picked=PhysicalToolIndex{1};
    vars.print_state=marlin_server::State::Printing; crash_s.state=Crash_s::REPLAY;
    // Interrupted command and all acknowledged-but-not-finished commands replay
    // in order. Earlier completed commands and the unconsumed ninth line do not.
    for(int i=2;i<8;++i) {
        assert(replay_one()); assert(replayed.back()==motion[i]);
        assert(feeds.back()==20+i && modes.back()==i);
    }
    assert(!pending() && !replay_one() && replayed.size()==6);
    crash_on_drain=false;
    assert(prepare("G1 X9") && drains==0 && waits==2);
    // Modal/tool/heater and non-supported motion commands are barriers.
    for(const char *command : {"M83", "G90", "M220 S50", "T2", "M976 A", "G28 XY",
        "G10", "G123 Z1", "G2.1 X2", "N22 M118 test*1", ""}) {
        assert(prepare(command)); assert(!crash_s.active && gcode_position(~0u)==~0u);
        assert(prepare("G1 X2")); assert(crash_s.active);
    }
    // Pause arriving during a barrier leaves the FIFO command unconsumed.
    pause_on_drain=true; assert(!prepare("M83")); pause_on_drain=false;
    // Overflow, bad snapshots and unsupported contexts fail closed.
    for(int failure=0;failure<6;++failure) {
        fresh(); assert(prepare("G1 X1 E.1")); crash_s.sdpos=gcode_position(~0u);
        if(failure==0) crash_s.sdpos=~0u;
        if(failure==1) crash_s.toolchange=true;
        if(failure==2) crash_s.recover_flags=0;
        if(failure==3) crash_s.crash_axes_home_level.known=false;
        if(failure==4) picked.reset();
        if(failure==5) crash_s.has_motion_snapshot=false;
        assert(!begin() && pending());
    }
    fresh(); assert(prepare("G1 X2")); crash_s.sdpos=gcode_position(~0u); assert(begin());
    crash_on_replay=true; assert(replay_one() && pending());
    assert(begin()); // a second collision retains the same command, not its successor
    crash_on_replay=false; assert(replay_one() && replayed[0]==replayed[1]);
    fresh(); marlin_server::serial=false;
    assert(prepare("G1 X2") && !crash_s.active && gcode_position(123)==123);
    fresh(); crash_s.enabled=false;
    assert(!monitoring_available() && prepare("G1 X2") && !crash_s.active && drains==0);
    fresh(); phase_stepping::enabled=true;
    assert(!monitoring_available() && prepare("G1 X2") && !crash_s.active && drains==0);
    fresh(); assert(prepare("G1 X2")); suspend();
    assert(!crash_s.active && gcode_position(~0u)==~0u);
    fresh(); assert(prepare("G1 X2 E.1"));
    const auto retained=gcode_position(~0u);
    for(const char *command : {"M73 P20 R100", "N4 M117 ETA soon*3", "M105", "M114", "M115"}) {
        assert(prepare(command));
        assert(drains==0 && waits==0 && gcode_position(~0u)==retained && crash_s.active);
    }
    // Repeated jobs/journal saturation stay bounded and use fresh identities.
    uint32_t previous=0;
    for(int job=0;job<1000;++job) {
        fresh();
        for(int line=0;line<33;++line) {
            live=true;
            assert(prepare("G1 X1"));
            uint32_t id=gcode_position(~0u); assert(id!=previous); previous=id;
        }
        assert(waits==4 && drains==0);
    }
}
''')

    def test_fifo_replay_precedes_injection_even_when_fifo_empty(self):
        source = (fixture.ROOT /
                  "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        advance = fixture.function(source, "void GCodeQueue::advance()")
        self.run_cpp(r'''
#include <cassert>
#define HAS_CRASH_DETECTION() 1
#define HAS_SERIAL_PRINT() 1
namespace buddy::serial_collision {
int replay=0, prepared=0; bool ready=true;
bool replay_one() { if(replay) { --replay; return true; } return false; }
bool prepare(const char*) { ++prepared; return ready; }
}
namespace marlin_server { bool held=false; bool serial_print_fifo_held() {return held;} }
int injected=0, executed=0;
void delay(int) {}
struct Queue {
 static constexpr unsigned SDPOS_INVALID=~0u;
 unsigned last_executed_sdpos=0;
 bool current_command_serial=false;
 int length=0,index_r=0,recovery_capacity=10,executed_commmand_count=0;
 char command_buffer[10][96]{};
 bool process_injected_command() { ++injected; return false; }
 unsigned get_current_sdpos() { return SDPOS_INVALID; }
 void advance();
} queue;
struct { void process_next_command() {++executed;} } gcode;
''' + advance.replace("GCodeQueue::advance", "Queue::advance") + r'''
int main() {
 using namespace buddy::serial_collision;
 replay=2; queue.advance(); queue.advance();
 assert(replay==0 && injected==0 && executed==0);
 queue.length=3; ready=false; queue.advance();
 assert(queue.length==3 && queue.index_r==0 && executed==0);
 ready=true; queue.advance();
 assert(queue.length==2 && queue.index_r==1 && executed==1);
 marlin_server::held=true; queue.advance();
 assert(queue.length==2 && executed==1);
}
''')


if __name__ == "__main__":
    unittest.main()

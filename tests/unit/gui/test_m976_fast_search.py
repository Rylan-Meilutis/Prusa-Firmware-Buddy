"""Execute the production optimizer with deterministic loadcell/fault fixtures.

This is not an INDX motion simulator. It executes the actual search/control
code, confidence/SNR functions and production Score layout under ASan/UBSan.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class FastSearch(unittest.TestCase):

    def test_actual_usage_timer_wrap_and_early_return(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        timer = "class ElapsedTime {" + source.split(
            "class ElapsedTime {", 1)[1].split("\n};", 1)[0] + "\n};\n"
        code = r'''
#include <cstdint>
#include <cassert>
uint32_t now;
uint32_t ticks_ms() { return now; }
'''
        main = r'''
void early_return(uint32_t &total) { ElapsedTime time(total); now+=25; }
int main() {
 uint32_t total=0;
 now=0xfffffff0;
 early_return(total);
 assert(total==25);
 early_return(total);
 assert(total==50);
 { ElapsedTime time(total); now+=10; }
 assert(total==60);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "timer.cpp").write_text(code + timer + main)
            binary = path / "timer"
            subprocess.run([
                "c++", "-std=c++20",
                str(path / "timer.cpp"), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

    def test_actual_indx_batch_cache_preparation_and_failures(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        entry = "struct BatchEntry {" + source.split(
            "struct BatchEntry {", 1)[1].split("\n};", 1)[0] + "\n};\n"
        batch = "bool run_batch(" + source.split(
            "bool run_batch(", 1)[1].split("#if !HAS_WASTEBIN()", 1)[0]
        code = r'''
#include <pa_temperature_model.hpp>
#include <m976_diagnostics.hpp>
#include <array>
#include <cassert>
#include <optional>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define HAS_INDX() 1
#define HAS_WASTEBIN() 1
#define PRUSA_MMU2 0
#define ENABLED(x) (x)
#define SERIAL_ECHOLNPAIR(...) ((void)0)
buddy::m976_diagnostics::History pa_diagnostics;
bool valid[8]{};
bool cache_all, fail, pickup_ok=true;
int pickups, parks;
std::vector<std::string> commands;
namespace buddy::extrusion_calibration {
constexpr size_t max_logical_filaments=8;
struct Result {};
Result *job_result(uint8_t slot) { static Result result; return valid[slot] ? &result : nullptr; }
void set_job_result(uint8_t slot, Result) { valid[slot]=false; }
}
struct PhysicalToolIndex {
 uint8_t raw;
 static PhysicalToolIndex from_raw(uint8_t raw) { return {raw}; }
 static std::optional<PhysicalToolIndex> currently_selected();
};
std::optional<PhysicalToolIndex> selected;
std::optional<PhysicalToolIndex> PhysicalToolIndex::currently_selected() { return selected; }
namespace stdext {
bool holds_value(std::optional<PhysicalToolIndex> current, PhysicalToolIndex requested) { return current && current->raw==requested.raw; }
}
enum class tool_return_t { no_return };
bool tool_change(PhysicalToolIndex tool, tool_return_t) { ++pickups; if(pickup_ok) selected=tool; return pickup_ok; }
struct Temperature { static void setTargetHotend(int,PhysicalToolIndex) {} };
struct DockFanGuard { DockFanGuard(size_t,int) {} };
struct FilamentSensorEventGuard {};
struct BlockEStallDetection {};
struct PressureMonitorGuard {};
struct CalibrationCommandGuard {};
struct MeasurementAnchorGuard { bool valid=true; explicit MeasurementAnchorGuard(uint8_t) {} };
bool restore_cache(uint8_t, uint8_t slot,int16_t) { if(cache_all) valid[slot]=true; return cache_all; }
void park_after_calibration() { ++parks; }
struct GcodeSuite {
 static void process_subcommands_now(const char *s) {
   commands.emplace_back(s);
   int tool, slot, temperature, forced;
   assert(sscanf(s,"M976 T%d L%d S%d F%d",&tool,&slot,&temperature,&forced)==4);
   valid[slot]=!fail;
   pa_diagnostics.records[slot].outcome=fail ? "failed" : "success";
 }
};
'''
        main = r'''
int main() {
 std::array<BatchEntry,8> entries{};
 entries[0].physical_tool=entries[0].logical_filament=0;
 entries[0].temperature=250;
 entries[1]=entries[0]; entries[1].temperature=240; entries[1].temperature_point=1;
 entries[2]=entries[0]; entries[2].physical_tool=entries[2].logical_filament=1;
 cache_all=true;
 assert(run_batch(entries,3,false,false));
 assert(commands.empty() && pickups==0);
 cache_all=false;
 assert(run_batch(entries,3,false,false));
 assert(commands.size()==3 && pickups==2);
 assert(commands[0].find(" P")==std::string::npos);
 assert(commands[1].find(" P")!=std::string::npos);
 assert(commands[2].find(" P")==std::string::npos);
 commands.clear();
 cache_all=true;
 assert(run_batch(entries,3,false,true));
 assert(commands.size()==3);
 for(const auto &s:commands) assert(s.find("F1")!=std::string::npos);
 cache_all=false; commands.clear(); fail=true;
 assert(!run_batch(entries,3,false,false));
 assert(commands.size()==1); // no later temperature after preparation failure
 fail=false; commands.clear(); selected.reset(); pickup_ok=false;
 assert(!run_batch(entries,3,false,false));
 assert(commands.empty());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "batch.cpp").write_text(code + entry + batch + main)
            binary = path / "batch"
            subprocess.run([
                "c++", "-std=c++20", "-fsanitize=address,undefined", "-I",
                str(ROOT / "src/common"),
                str(path / "batch.cpp"), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_actual_batch_order_preserves_mmu_and_bounds_indx_plans(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        entry = "struct BatchEntry {" + source.split(
            "struct BatchEntry {", 1)[1].split("\n};", 1)[0] + "\n};\n"
        schedule = "size_t rounds = 1;" + source.split(
            "size_t rounds = 1;", 1
        )[1].split(
            "        for (size_t i = 0; i < count; ++i) {\n            buddy::extrusion_calibration::request_job_temperature",
            1)[0]
        code = r'''
#include <pa_temperature_model.hpp>
#include <m976_diagnostics.hpp>
#include <array>
#include <cassert>
#include <tuple>
#include <vector>
#define HAS_INDX() INDX
#define SERIAL_ERROR_MSG(...) ((void)0)
#define SERIAL_ECHOLN(...) ((void)0)
namespace buddy::extrusion_calibration { constexpr size_t max_logical_filaments=8; }
buddy::m976_diagnostics::History pa_diagnostics;
std::vector<std::tuple<int,int>> visited;
std::vector<int> temperatures;
bool fail=false;
'''
        fixture = r'''
bool run_batch(const std::array<BatchEntry,8> &entries, size_t count, bool, bool) {
 assert(count<=8);
 for(size_t i=0;i<count;++i) {
   auto &r=pa_diagnostics.records[entries[i].logical_filament];
   if(entries[i].temperature_point && std::string_view(r.outcome)=="fallback") continue;
   visited.emplace_back(entries[i].logical_filament,entries[i].temperature_point);
   temperatures.push_back(entries[i].temperature);
   r.outcome=fail ? "fallback" : "success";
 }
 return true;
}
void schedule_plan(std::array<BatchEntry,8> &entries,size_t count) {
 const bool force=false;
'''
        main = r'''
}
int main() {
 for(int tools=1;tools<=8;++tools) for(int points=1;points<=8;++points) {
   std::array<BatchEntry,8> entries{};
   for(int i=0;i<tools;++i) {
     entries[i].physical_tool=entries[i].logical_filament=i;
     entries[i].temperature_count=points;
     entries[i].temperature=250;
     for(int j=0;j<points;++j) entries[i].temperatures[j]=250-j;
   }
   visited.clear(); temperatures.clear(); fail=false; pa_diagnostics={};
   schedule_plan(entries,tools);
   assert(visited.size()==size_t(tools*points));
   for(int i=0;i<tools;++i) assert(entries[i].temperature==250); // first-layer selection is unchanged
   for(int index=0;index<tools*points;++index) {
     const int tool=INDX ? index/points : index%tools;
     const int point=INDX ? index%points : index/tools;
     assert(visited[index]==std::make_tuple(tool,point));
     assert(temperatures[index]==(INDX ? 250-points+1+point : 250-point));
   }
   visited.clear(); fail=true; pa_diagnostics={};
   schedule_plan(entries,tools);
   assert(visited.size()<=size_t(tools));
   for(const auto &[tool,point]:visited) assert(point==0);
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "plan.cpp").write_text(code + entry + fixture + schedule +
                                           main)
            for indx in (0, 1):
                binary = path / "plan"
                subprocess.run([
                    "c++", "-std=c++20", "-fsanitize=address,undefined",
                    f"-DINDX={indx}", "-I",
                    str(ROOT / "src/common"),
                    str(path / "plan.cpp"), "-o",
                    str(binary)
                ],
                               check=True)
                subprocess.run([str(binary)],
                               check=True,
                               env=dict(os.environ,
                                        ASAN_OPTIONS="detect_leaks=0"))

    def test_actual_bursts_cleanup_reuse_and_faults(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        bursts = "buddy::extrusion_calibration::Score run_bursts(" + source.split(
            "buddy::extrusion_calibration::Score run_bursts(", 1)[1].split(
                "float measure_idle_noise_floor()", 1)[0]
        code = r'''
#include <m976_indx_pellet_policy.hpp>
#include <m976_extrusion_policy.hpp>
#include <m976_diagnostics.hpp>
#include <cassert>
#include <cmath>
#define HAS_INDX() 1
#define SERIAL_ERROR_MSG(...) ((void)0)
#define SERIAL_ECHOLN(...) ((void)0)
constexpr float filament_area=2.40528f;
buddy::m976_diagnostics::History pa_diagnostics;
struct ElapsedTime { explicit ElapsedTime(uint32_t &) {} };
bool flexible, start_ok=true, eject_ok=true;
int clean_calls, fail_clean=-1, ejections, extrusions, releases;
bool capturing;
namespace buddy::extrusion_calibration {
struct Score {};
struct Capture {
 bool start() { capturing=start_ok; return start_ok; }
 void pause() { capturing=false; }
 void resume() { capturing=true; }
 void resume_on_motion(float) { capturing=true; }
 void stop() { capturing=false; }
 void release() { ++releases; }
 Score score(bool) { return {}; }
};
Capture &capture() { static Capture capture; return capture; }
}
namespace pressure_advance {
struct Config { float pa, smooth_time; };
Config get_axis_e_config() { return {}; }
void set_axis_e_config(Config) {}
void set_calibration_mode(bool) {}
}
struct { bool stopped=false; bool draining() { return stopped; } void synchronize() {} float get_axis_position_mm(int) {return 0;} } planner;
#define E_AXIS 0
namespace nozzle_cleaner {
enum class Sequence { pa_calibration_extrusion_position };
bool load_and_execute(Sequence) { assert(!capturing); return clean_calls++ != fail_clean; }
}
bool flexible_calibration(uint8_t) { return flexible; }
auto measurement_speeds(uint8_t) {return buddy::m976_extrusion_policy::speeds(flexible);}
void extrude_flow(float, float) { assert(capturing); ++extrusions; }
bool eject_accumulated_indx_pellet(buddy::m976_indx_pellet_policy::Budget &budget, uint8_t) {
 assert(!capturing);
 ++ejections;
 if(eject_ok) budget={};
 return eject_ok;
}
'''
        main = r'''
int main() {
 using buddy::m976_indx_pellet_policy::Budget;
 Budget budget;
 bool ready=true;
 for(int measurement=0;measurement<5;++measurement) run_bursts(.04f,budget,ready,0);
 assert(extrusions==40 && ejections==4 && releases==5 && clean_calls==3);
 assert(!budget.cycles && !capturing);
 assert(std::abs(pa_diagnostics.records[0].excitation_mm-48.f)<.001f);
 start_ok=false;
 run_bursts(.04f,budget,ready,0);
 assert(extrusions==40 && releases==5 && !capturing);
 start_ok=true; ready=false; fail_clean=clean_calls;
 run_bursts(.04f,budget,ready,0);
 assert(extrusions==40 && releases==6 && !capturing);
 fail_clean=-1; ready=true; budget={4,9.6f}; eject_ok=false;
 run_bursts(.04f,budget,ready,0);
 assert(extrusions==42 && releases==7 && !capturing);
 // Failed cleanup retains the budget; no more extrusion is permissible.
 run_bursts(.04f,budget,ready,0);
 assert(extrusions==42 && releases==8 && !capturing);
 eject_ok=true; budget={}; flexible=true;
 const int before=extrusions;
 run_bursts(.04f,budget,ready,0);
 assert(extrusions==before+8 && budget.cycles==4 && budget.filament_mm<2);
 planner.stopped=true;
 run_bursts(.04f,budget,ready,0);
 assert(extrusions==before+8 && !capturing);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "bursts.cpp").write_text(code + bursts + main)
            binary = path / "bursts"
            subprocess.run([
                "c++", "-std=c++20", "-fsanitize=address,undefined", "-I",
                str(ROOT / "src/common"),
                str(path / "bursts.cpp"), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_actual_search_fault_matrix(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        header = (ROOT / "src/feature/extrusion_calibration.hpp").read_text()
        score = "struct Score {" + header.split("struct Score {", 1)[1].split(
            "\n};", 1)[0] + "\n};"
        confidence = "float result_confidence(" + source.split(
            "float result_confidence(", 1)[1].split(
                "void report_measurement_debug", 1)[0]
        search = "float best_pa =" + source.split(
            "float best_pa =", 1)[1].split(
                "    #if HAS_INDX()\n    // Bound buildup", 1)[0]
        prelude = r'''
#include <m976_search_policy.hpp>
#include <m976_indx_pellet_policy.hpp>
#include <m976_diagnostics.hpp>
#include <array>
#include <cassert>
#include <limits>
#include <string_view>
#include <cstdio>
namespace buddy::extrusion_calibration {
'''
        fixture = r'''
}
using buddy::extrusion_calibration::Score;
constexpr float absolute_pa_max = .5f;
struct CandidateObservation { float pa, cost; };
std::array<CandidateObservation,26> candidate_observations;
size_t candidate_observation_count;
enum class PhasesPressureAdvanceCalibration { measuring };
void pa_fsm_change(PhasesPressureAdvanceCalibration, uint8_t, uint8_t) {}
int calls, stop_at, fault, fault_at;
float optimum;
bool pa_abort_requested(PhasesPressureAdvanceCalibration) { return calls == stop_at; }
void report_measurement_debug(float, const Score &, float) {}
float material_fallback(uint8_t) { return .04f; }
Score run_bursts(float candidate, buddy::m976_indx_pellet_policy::Budget &, bool &, uint8_t) {
 assert(candidate >= 0 && candidate <= .5f && std::isfinite(candidate));
 assert(calls < 27); // 4 warm + 13 full + at most 10 confirmations.
 const int index = calls++;
 Score score;
 score.valid = true; score.mean_load = 100; score.noise = 1;
 score.sample_count = 975; score.transitions_used = score.transitions_detected = 4;
 score.strongest_transition = 100; score.highest_transition_noise = 1;
 score.transient_stddev = .01f;
 score.transient = .1f + 10000 * (candidate-optimum)*(candidate-optimum);
 if (index >= fault_at) switch(fault) {
 case 1: score.valid = false; score.strongest_transition = 1; break;
 case 2: score = {}; break;
 case 3: score.capture_overflow = true; score.valid = false; break;
 case 4: score.transient_stddev = 10; break;
 case 5: score.transient = NAN; score.valid = false; break;
 case 6: score.transient = .1f; break; // flat/unidentifiable curve
 case 7: score.valid = false; score.rejected_timing = 4; break;
 case 8: score.transient += index % 2 ? .09f : 0.f; break;
 }
 return score;
}
struct Outcome { float pa; bool quick, aborted, failed; int calls; const char *path; };
Outcome run(float hint, bool warm_start, bool force, int retries=6, float noise=1.f) {
 calls=0;
 float search_default=hint;
 const uint8_t slot=0;
 const float idle_noise=noise, minimum_result_confidence=.75f, minimum_snr=6;
 const uint8_t maximum_confidence_retries=std::clamp(retries,0,10);
 buddy::m976_diagnostics::Record diagnostic;
'''
        tail = r'''
 return {best_pa, quick_verified, aborted, signal_guard.failure != nullptr, calls, diagnostic.search};
}
int main() {
 optimum=.04f; stop_at=-1; fault=0; fault_at=0;
 auto r=run(.04f,true,false);
 assert(r.quick && r.calls==4 && !r.failed && std::abs(r.pa-optimum)<.0001f);
 r=run(.04f,true,true);
 assert(!r.quick && r.calls<=13 && r.calls>=10);
 optimum=.052f;
 r=run(.04f,true,false);
 assert(!r.quick && r.calls<=17 && std::abs(r.pa-optimum)<.003f);
 for(int i=0;i<=30;++i) { // temperatures/optima away from the hint
   optimum=.015f+i*.0015f;
   r=run(.04f,true,false);
   assert(r.calls<=17 && !r.failed && std::abs(r.pa-optimum)<.003f);
 }
 optimum=.04f;
 for(int kind=1;kind<=8;++kind) {
   fault=kind;
   for(int at=0;at<17;++at) {
     fault_at=at;
     for(int retry : {0,1,6,10}) {
       r=run(.04f,true,false,retry);
       assert(r.calls<=27);
       if(at==0 && kind==1) assert(r.failed && r.calls==3);
       if(at==0 && (kind==2 || kind==3)) assert(r.failed && r.calls==1);
       if(at==0 && kind>=4 && kind<=7) assert(!r.quick);
     }
   }
 }
 fault=0; fault_at=0;
 for(int at=0;at<=27;++at) {
   stop_at=at;
   r=run(.08f,true,false,10);
   assert(r.calls<=at);
   if(at<4) assert(r.aborted && !r.quick);
 }
 stop_at=-1;
 r=run(.04f,true,false,10,NAN);
 assert(r.failed && r.calls==0);
 for(float edge : {0.f,.001f,.499f,.5f}) {
   optimum=edge; r=run(edge,true,false);
   assert(!r.quick && r.calls<=27);
 }
 puts("production PA search fault matrix passed");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp = path / "search.cpp"
            # Confidence functions must precede the wrapper containing search.
            wrapper = fixture.index("struct Outcome")
            cpp.write_text(prelude + score + fixture[:wrapper] + confidence +
                           fixture[wrapper:] + search + tail)
            binary = path / "search"
            subprocess.run([
                "c++", "-std=c++20", "-g", "-fsanitize=address,undefined",
                "-I",
                str(ROOT / "src/common"),
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))


if __name__ == "__main__":
    unittest.main()

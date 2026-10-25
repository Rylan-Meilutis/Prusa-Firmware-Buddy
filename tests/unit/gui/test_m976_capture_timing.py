"""Run actual M976 bursts and capture with deterministic planner timing.

The producer runs at INDX's actual 366 Hz; injected feed/flow limits and scheduling latency
are explicit scenarios, not a claim to reproduce the user's physical printer.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class CaptureTiming(unittest.TestCase):

    def test_production_capture_and_bursts_under_retained_motion_limits(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        bursts = "buddy::extrusion_calibration::Score run_bursts(" + source.split(
            "buddy::extrusion_calibration::Score run_bursts(", 1)[1].split(
                "float measure_idle_noise_floor()", 1)[0]
        flow = "void extrude_flow(" + source.split(
            "void extrude_flow(", 1)[1].split("\n#if HAS_INDX()", 1)[0]
        speed_policy = "buddy::m976_extrusion_policy::Speeds measurement_speeds(" + source.split(
            "buddy::m976_extrusion_policy::Speeds measurement_speeds(",
            1)[1].split("buddy::extrusion_calibration::Score run_bursts(",
                        1)[0]
        capture_source = (
            ROOT / "src/common/feature/extrusion_calibration.cpp").read_text()
        capture = "bool Capture::start()" + capture_source.split(
            "bool Capture::start()", 1)[1].split("\nCapture &capture()", 1)[0]
        planner_source = (
            ROOT / "lib/Marlin/Marlin/src/module/planner.cpp").read_text()
        flow_limit = "if (delta_mm.e > 0 && std::isfinite(max_volumetric_flow_mm3_s[extruder])) {" + planner_source.split(
            "if (delta_mm.e > 0 && std::isfinite(max_volumetric_flow_mm3_s[extruder])) {",
            1)[1].split("\n    #ifdef COREXY", 1)[0]
        harness = r'''
#include <feature/extrusion_calibration.hpp>
#include <m976_indx_pellet_policy.hpp>
#include <m976_extrusion_policy.hpp>
#include <m976_diagnostics.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <new>
#include <vector>
#define HAS_INDX() 1
#define SERIAL_ERROR_MSG(...) ((void)0)
#define SERIAL_ECHOLN(...) ((void)0)
#define NOMORE(x,y) (x)=std::min((x),(y))
namespace buddy { struct InterruptDisabler {}; }
namespace buddy::extrusion_calibration {
''' + capture + r'''
Capture &capture() { static Capture instance; return instance; }
}
constexpr float filament_area = 2.40528f;
buddy::m976_diagnostics::History pa_diagnostics;
struct ElapsedTime { explicit ElapsedTime(uint32_t &) {} };
bool flexible = false;
double now_us = 0, next_sample_us = 0;
float e_position = 0, max_e_speed = 100, flow_cap = INFINITY;
unsigned drain_delay_us = 0, drain_tail_us = 0;
void sample_for(double duration_us, float velocity = 0) {
    const double until = now_us + duration_us;
    const float start_e = e_position;
    while (next_sample_us < until) {
        e_position = start_e + velocity * (next_sample_us - now_us) / 1000000.0;
        buddy::extrusion_calibration::capture().record(
            uint32_t(next_sample_us), velocity * 10, e_position);
        next_sample_us += 1000000.0 / 366.0;
    }
    e_position = start_e + velocity * duration_us / 1000000.0;
    now_us = until;
}
float actual_speed(float distance, float requested) {
    constexpr unsigned extruder=0;
    struct { float e; } delta_mm {distance};
    struct { std::optional<unsigned> virtual_tool {0}; } tools;
    std::array<float, 8> max_volumetric_flow_mm3_s;
    max_volumetric_flow_mm3_s.fill(flow_cap);
    float inverse_secs = requested / distance;
    float speed_factor = 1;
''' + flow_limit + r'''
    return std::min(requested * speed_factor, max_e_speed);
}
struct Move { float distance, speed; };
std::vector<Move> moves;
namespace mapi {
bool extruder_move(float distance, float speed, bool) {
    moves.push_back({distance, speed}); return true;
}
}
struct {
    struct { float max_feedrate_mm_s[1] {100}; unsigned max_acceleration_mm_per_s2[1] {10000}; float retract_acceleration = 10000; } settings;
    std::array<float,8> max_volumetric_flow_mm3_s;
    float get_axis_position_mm(int) {return e_position;}
    bool draining() { return false; }
    void synchronize() {
        if (!moves.empty()) sample_for(drain_delay_us);
        for (const auto move : moves) {
            float speed = actual_speed(move.distance, move.speed);
            sample_for(double(move.distance) / speed * 1000000.0, speed);
        }
        if (!moves.empty()) sample_for(drain_tail_us);
        moves.clear();
    }
} planner;
#define E_AXIS 0
#define E_AXIS_N(x) 0
constexpr int active_extruder=0;
#define EXTRUDERS 8
struct VirtualToolIndex {
    static std::optional<VirtualToolIndex> currently_selected() {return VirtualToolIndex{};}
    operator size_t() const {return 0;}
};
namespace stdext { template<class T> auto get_optional(std::optional<T> value) {return value;} }
namespace pressure_advance {
struct Config { float pa, smooth_time; };
Config get_axis_e_config() { return {}; }
void set_axis_e_config(Config) {}
void set_calibration_mode(bool) {}
}
namespace nozzle_cleaner {
enum class Sequence { pa_calibration_extrusion_position };
bool load_and_execute(Sequence) { sample_for(1000000); return true; }
}
bool flexible_calibration(uint8_t) { return flexible; }
bool eject_accumulated_indx_pellet(buddy::m976_indx_pellet_policy::Budget &budget, uint8_t) {
    sample_for(15000000); // Real cooling duration; capture must be paused.
    budget = {}; return true;
}
''' + flow + speed_policy + bursts + r'''
buddy::extrusion_calibration::Score measure(unsigned delay, float e_cap, float volume_cap) {
    drain_delay_us = delay; max_e_speed = e_cap; flow_cap = volume_cap;
    planner.settings.max_feedrate_mm_s[0]=e_cap;
    planner.max_volumetric_flow_mm3_s.fill(volume_cap);
    buddy::m976_indx_pellet_policy::Budget budget;
    bool ready = true;
    return run_bursts(.04f, budget, ready, 1);
}
int main() {
    const auto nominal = measure(0, 100, INFINITY);
    assert(nominal.sample_count >= 1096 && nominal.sample_count <= 1102);
    assert(!nominal.capture_overflow);
    // Count is reset every attempt, even with many preceding measurements.
    for (int job = 0; job < 2; ++job) for (int candidate = 0; candidate < 5; ++candidate) {
        auto score = measure(0, 100, INFINITY);
        assert(!score.capture_overflow && score.sample_count < 1120);
    }
    // Retained second-job E/flow caps keep their limits but no longer stretch
    // the excitation duration beyond capture capacity.
    auto capped = measure(0, 4, INFINITY);
    assert(!capped.capture_overflow && capped.valid);
    // Conservative ASA flow cap plus 150 ms per queue drain also overruns.
    auto delayed = measure(150000, 100, 12);
    assert(!delayed.capture_overflow && delayed.valid);
    // The real service loop may return long after E reaches the endpoint.
    // Neither the production 366 Hz rate nor post-motion scheduling may turn
    // an otherwise valid ASA measurement into capture_overflow.
    drain_tail_us = 2000000;
    auto tail = measure(150000, 100, 12);
    assert(!tail.capture_overflow && tail.valid && tail.sample_count < 1120);
    planner.settings.retract_acceleration = 10;
    auto soft = measure(150000, 100, 12);
    assert(!soft.capture_overflow);
    planner.settings.retract_acceleration = 0;
    assert(!measure(0, 100, INFINITY).valid);
    planner.settings.retract_acceleration = 10000;
    // Scheduling alone can exceed the claimed 30% buffer margin.
    auto scheduled = measure(300000, 100, INFINITY);
    assert(!scheduled.capture_overflow && scheduled.valid);
    auto severe_wait = measure(3000000, 2, 6);
    assert(!severe_wait.capture_overflow && severe_wait.valid);
    assert(!measure(0, 0, INFINITY).valid);
    assert(!measure(0, 100, 0).valid);
    planner.settings.max_acceleration_mm_per_s2[0]=0;
    assert(!measure(0, 100, INFINITY).valid);
    planner.settings.max_acceleration_mm_per_s2[0]=10000;
    // A subsequent nominal capture recovers: no cross-job accumulation.
    auto recovered = measure(0, 100, INFINITY);
    assert(!recovered.capture_overflow && recovered.sample_count < 1120);
    // TPU's slower requested excitation fits even under a conservative cap.
    flexible = true;
    auto tpu = measure(0, 4, 2.4f);
    assert(!tpu.capture_overflow && tpu.valid);
    // Exercise material routing as well as both actual burst/scoring paths.
    // These deterministic signals validate firmware logic, not polymer quality.
    for (const auto material : {"PLA", "PETG", "ASA", "ABS", "HIPS", "PC", "PA", "PVA",
                                "TPU", "TPE", "FLEX", "TPU-95A"}) {
        flexible = buddy::m976_extrusion_policy::is_flexible("PROFILE-001", material, false);
        for (const float cap : {6.f, 12.f, 24.f}) {
            auto score = measure(300000, 4, flexible ? 2.4f : cap);
            assert(!score.capture_overflow && score.valid);
        }
        std::printf("material path verified: %s (%s)\n", material, flexible ? "flexible" : "rigid");
    }
    std::printf("production capture: nominal=%u E-cap=%u flow/delay=%u TPU=%u; overflow regression fixed\n",
        nominal.sample_count, capped.sample_count, delayed.sample_count, tpu.sample_count);
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            (path / "timing.cpp").write_text(harness)
            subprocess.run([
                "c++", "-std=c++20", "-fsanitize=address,undefined", "-I",
                str(ROOT / "src"), "-I",
                str(ROOT / "src/common"),
                str(path / "timing.cpp"), "-o",
                str(path / "timing")
            ],
                           check=True)
            subprocess.run([str(path / "timing")],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))


if __name__ == "__main__":
    unittest.main()

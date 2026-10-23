"""Execute the production dialog classifier and light update without hardware."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class HeaterTimeoutLighting(unittest.TestCase):

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_timeout_warning_allows_dim_and_off_without_dismissing(self):
        source = (ROOT / 'src/leds/side_strip_handler.cpp').read_text()
        code = r'''
#include <array>
#include <optional>
#include <mutex>
#include <cassert>
#include <cstdint>
#include <rme_light_mode.hpp>
using std::size_t;
enum class WarningType : uint8_t { NozzleTimeout, HeatersTimeout, HotendFanError };
enum class ClientFSM { Printing, Workflow, Warning, Serial_printing, _count };
namespace fsm {
struct BaseData {
    std::array<uint8_t,4> data{};
    auto GetData() const { return data; }
};
struct States {
    std::array<std::optional<BaseData>,4> data{};
    const auto &operator[](ClientFSM type) const { return data[size_t(type)]; }
};
}
namespace marlin_server {
enum class State { Idle, PrintInit, SerialPrintInit, Printing, Paused,
                   Finishing_WaitIdle, Finished, Aborted, Exit };
bool serial=false;
bool serial_print_active() { return serial; }
bool is_printing_state(State s) { return s==State::Printing; }
bool is_extended_paused_state(State s) { return s==State::Paused; }
bool finishing_or_finished();
}
struct Vars {
    struct { marlin_server::State value=marlin_server::State::Idle;
        auto get() const {return value;} } print_state;
    fsm::States states;
    template<class F> auto peek_fsm_states(F f) { return f(states); }
} vars;
auto &marlin_vars() { return vars; }
bool marlin_server::finishing_or_finished() {
    return vars.print_state.value==State::Finished;
}
uint32_t now=1000;
uint32_t ticks_ms() { return now; }
struct SimpleTransitionController { void update() {} } controller;
auto &controller_instance() { return controller; }
enum class SideStripState { unknown, off, dimmed, active, printing, idle, custom_color };
enum class LightState { active, idle, deep_idle, printing };
''' + function(source,
               'static bool print_active_for_leds()') + '\n' + function(
                   source,
                   'static bool guided_activity_active()') + '\n' + function(
                       source,
                       'static LightState light_state_for_strip_state(') + r'''
bool startup_activity_within_window(uint32_t start, uint32_t time, uint32_t duration) {
    return uint32_t(time-start)<duration;
}
constexpr uint32_t startup_activity_duration_ms=30000;
bool machine_operation_holds_active(bool print, bool terminal, bool host, bool held) {
    return !print && !host && held;
}
struct SideStripHandler {
    std::mutex mutex;
    rme_light_mode::State chamber_mode_state;
    rme_light_hold::State rme_hold;
    rme_light_mode::PrintState print_chamber_mode;
    struct Color { uint32_t duration_ms, start_ms; };
    std::optional<Color> custom_color;
    bool door_open_for_leds=false, door_holds_active=true, host_idle_override=false;
    bool startup_activity_active=false, print_override_session_active=false;
    bool print_brightness_overridden=false, print_screen_brightness_overridden=false;
    bool screen_brightness_wake_from_print_override=false, screen_forced_off=false;
    bool print_or_filter_active_prev=false, post_print_hold=false;
    bool post_print_hold_enabled=false, post_print_hold_dismissed=false;
    bool post_print_hold_seen_door_open=false;
    unsigned operation_hold_count=0;
    uint32_t startup_activity_started_ms=0, screen_brightness_wake_until_ms=0;
    uint32_t active_timestamp_ms=1000;
    uint16_t activity_timeout_s=10, off_timeout_s=20;
    SideStripState state=SideStripState::active;
    void change_state(SideStripState next) {state=next;}
    void restart_idle_countdown(uint32_t time) {active_timestamp_ms=time;}
    void update();
};
''' + function(source, 'void SideStripHandler::update()') + r'''
int main() {
    for (auto warning : {WarningType::NozzleTimeout, WarningType::HeatersTimeout}) {
        vars.states.data={}; vars.print_state.value=marlin_server::State::Idle;
        vars.states.data[size_t(ClientFSM::Warning)]=fsm::BaseData{{uint8_t(warning)}};
        assert(!guided_activity_active());
        SideStripHandler lights;
        for (now=1000; now<11000; now+=1000) {
            lights.update(); assert(lights.state==SideStripState::active);
            assert(lights.active_timestamp_ms==1000);
        }
        lights.update(); assert(lights.state==SideStripState::dimmed);
        assert(light_state_for_strip_state(lights.state)==LightState::idle);
        now=31000; lights.update(); assert(lights.state==SideStripState::off);
        assert(light_state_for_strip_state(lights.state)==LightState::deep_idle);
        assert(vars.states.data[size_t(ClientFSM::Warning)]->GetData()[0]==uint8_t(warning));
        // Real touch/activity wakes normally, even while the warning remains.
        lights.active_timestamp_ms=now; lights.update();
        assert(lights.state==SideStripState::active);
        // A workflow under the warning must not lose its active hold.
        vars.states.data[size_t(ClientFSM::Workflow)]=fsm::BaseData{};
        now+=60000; lights.update(); assert(lights.state==SideStripState::active);
        assert(lights.active_timestamp_ms==now);
        vars.states.data[size_t(ClientFSM::Workflow)].reset();
        now+=10000; lights.update(); assert(lights.state==SideStripState::dimmed);
        // Door and machine-operation holds remain authoritative.
        lights.door_open_for_leds=true; lights.update();
        assert(lights.state==SideStripState::active);
        lights.door_open_for_leds=false; lights.operation_hold_count=1; lights.update();
        assert(lights.state==SideStripState::active);
        lights.operation_hold_count=0;
        // Paused and running jobs keep the configured print-light policy.
        for (auto s : {marlin_server::State::Paused, marlin_server::State::Printing}) {
            vars.print_state.value=s; lights.update();
            assert(lights.state==SideStripState::printing);
        }
    }
    vars.print_state.value=marlin_server::State::Idle;
    vars.states.data={}; assert(!guided_activity_active());
    vars.states.data[size_t(ClientFSM::Warning)]=fsm::BaseData{{uint8_t(WarningType::HotendFanError)}};
    assert(guided_activity_active());
    SideStripHandler fault;
    now+=60000; fault.update(); assert(fault.state==SideStripState::active);
    // Retaining a local/serial result screen never owns an activity hold.
    // This includes a result FSM outliving the terminal server state.
    for (auto result : {marlin_server::State::Aborted, marlin_server::State::Finished,
                        marlin_server::State::Idle, marlin_server::State::Exit}) {
        for (auto screen : {ClientFSM::Printing, ClientFSM::Serial_printing}) {
            vars.states.data={}; vars.print_state.value=marlin_server::State::Printing;
            vars.states.data[size_t(screen)]=fsm::BaseData{};
            SideStripHandler light;
            light.post_print_hold_enabled=true;
            now=1000; light.update(); assert(light.state==SideStripState::printing);
            vars.print_state.value=result;
            now=2000; light.update();
            auto timestamp=light.active_timestamp_ms;
            now=timestamp+10000; light.update(); assert(light.state==SideStripState::dimmed);
            now=timestamp+30000; light.update(); assert(light.state==SideStripState::off);
            assert(vars.states.data[size_t(screen)].has_value());
            // A real warning above a result screen still holds the lights.
            vars.states.data[size_t(ClientFSM::Warning)]=fsm::BaseData{{uint8_t(WarningType::HotendFanError)}};
            light.update(); assert(light.state==SideStripState::active);
        }
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(code)
            executable = path / 'test'
            subprocess.run([
                'c++', '-std=c++20', '-fsanitize=address,undefined',
                '-fno-pie', '-no-pie', '-I' + str(ROOT / 'src/common'),
                str(path / 'test.cpp'), '-o',
                str(executable)
            ],
                           check=True)
            # LSan cannot operate in the ptrace sandbox; keep ASan/UBSan on.
            subprocess.run([str(executable)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))


if __name__ == '__main__':
    unittest.main()

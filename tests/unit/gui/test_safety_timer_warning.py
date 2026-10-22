"""Execute the production safety-timeout handler with parked/picked tools.

Heater shutdown and restore bookkeeping must not depend on whether a warning
is shown. No physical heaters or printer connection are used by this fixture.
"""
import unittest

import test_serial_pause_recovery as fixture


class SafetyTimerWarnings(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_timeout_warning_and_shutdown_matrix(self):
        source = (fixture.ROOT /
                  "src/feature/safety_timer/safety_timer.cpp").read_text()
        trigger = fixture.function(source, "void SafetyTimer::trigger()")
        harness = r'''
#include <array>
#include <cassert>
#include <optional>
#define debug_assert assert
#define HAS_HUMAN_INTERACTIONS() UI
#define PRINTER_IS_PRUSA_iX() IX
#define ACTION_ON_SAFETY_TIMER_EXPIRED 1
enum class WarningType { NozzleTimeout, HeatersTimeout };
int selected=-1, bed_target=0, warning_count=0, host_events=0;
int hotend_disables=0, all_disables=0, display_clears=0;
bool processing=false, printing=false;
std::optional<WarningType> warning;
struct PhysicalToolIndex {
    static std::array<int, 8> all() { return {0,1,2,3,4,5,6,7}; }
    static std::optional<int> currently_selected_opt() {
        return selected < 0 ? std::nullopt : std::optional<int>(selected);
    }
};
struct Hotend {
    int target=0;
    static Hotend &for_tool(int i);
    int nozzle_target_temp() const { return target; }
};
std::array<Hotend,8> hotends{};
Hotend &Hotend::for_tool(int i) { return hotends.at(i); }
struct Temperature {
    static int degTargetBed() { return bed_target; }
    static void disable_hotend() {
        ++hotend_disables;
        for(auto &h : hotends) h.target=0;
    }
    static void disable_all_heaters() {
        ++all_disables; disable_hotend(); bed_target=0;
    }
};
namespace utils { enum class TimerState { running, finished }; }
struct Timer {
    bool expired=false, stopped=false;
    utils::TimerState state() const {
        return expired ? utils::TimerState::finished : utils::TimerState::running;
    }
    void stop() { stopped=true; }
};
struct AutoRestore {
    bool &value; bool old;
    AutoRestore(bool &v,bool next) : value(v),old(v) { v=next; }
    ~AutoRestore() { value=old; }
};
void bsod_unreachable() { assert(false); }
void host_action_safety_timer_expired() { ++host_events; }
void clear_temp_to_display() { ++display_clears; }
namespace marlin_server {
enum class State { Idle, Finished };
bool is_processing() { return processing; }
bool is_printing() { return printing; }
void set_warning(WarningType w) { ++warning_count; warning=w; }
}
struct { marlin_server::State print_state=marlin_server::State::Idle; } vars;
auto &marlin_vars() { return vars; }
class SafetyTimer {
public:
    enum class State { idle, active, restoring };
    bool prevent_recursion_=false, bed_timer_enabled_=true;
    unsigned blocker_count_=0;
    Timer activity_timer_, bed_activity_timer_;
    State state_=State::idle;
    std::array<int,8> nozzle_temperatures_to_restore_{};
    void load_config() {}
    void trigger();
};
''' + trigger + r'''
int main() {
    // Full matrix: idle/active print, no tool/every picked physical tool,
    // targets only on docked tools vs on picked tool, bed on/off, timer on/off.
    for (bool active : {false,true})
    for (int picked=-1; picked<8; ++picked)
    for (bool picked_heated : {false,true})
    for (bool parked_heated : {false,true})
    for (bool bed_on : {false,true})
    for (bool bed_expired : {false,true})
    for (bool bed_enabled : {false,true}) {
        selected=picked; printing=active; processing=false;
        warning.reset(); warning_count=host_events=0;
        hotend_disables=all_disables=display_clears=0;
        hotends={}; bed_target=bed_on ? 110 : 0;
        if(parked_heated) hotends[picked==0 ? 1 : 0].target=265;
        if(picked>=0 && picked_heated) hotends[picked].target=220;
        const auto before=hotends;
        SafetyTimer timer;
        timer.bed_timer_enabled_=bed_enabled;
        timer.bed_activity_timer_.expired=bed_expired;
        timer.trigger();
        const bool nozzle_off=picked>=0 && picked_heated;
        const bool bed_off=!active && bed_on && bed_expired && bed_enabled;
        const bool notify=nozzle_off || bed_off;
        assert(host_events==int(notify));
        assert(warning_count==(UI ? int(notify) : 0));
        if(UI && notify) assert(warning==
            (bed_off ? WarningType::HeatersTimeout : WarningType::NozzleTimeout));
        bool any_hotend=false;
        for(auto h : before) any_hotend |= h.target>0;
        if(any_hotend || bed_off) {
            for(auto h : hotends) assert(h.target==0);
            assert(hotend_disables==1 && display_clears==1);
        } else assert(hotend_disables==0 && display_clears==0);
        assert(bed_target==(bed_on && !bed_off ? 110 : 0));
        if(active && any_hotend) {
            assert(timer.state_==SafetyTimer::State::active);
            for(int i=0;i<8;++i)
                assert(timer.nozzle_temperatures_to_restore_[i]==before[i].target);
        }
        assert(!timer.prevent_recursion_);
    }
    // iX Finished must not cool the bed or claim that it did.
    if(IX) {
        printing=false; selected=-1; hotends={}; bed_target=110;
        warning_count=host_events=0;
        vars.print_state=marlin_server::State::Finished;
        SafetyTimer timer; timer.bed_activity_timer_.expired=true;
        timer.trigger();
        assert(bed_target==110 && warning_count==0 && host_events==0);
    }
    // Processing outside a print uses the same restorable shutdown path.
    vars.print_state=marlin_server::State::Idle;
    processing=true; printing=false; selected=0; hotends={};
    hotends[0].target=210; warning_count=host_events=0;
    SafetyTimer timer; timer.trigger();
    assert(timer.state_==SafetyTimer::State::active && host_events==1);
    assert(timer.nozzle_temperatures_to_restore_[0]==210);
}
'''
        for ui, ix in ((1, 0), (0, 0), (1, 1)):
            with self.subTest(ui=ui, ix=ix):
                self.run_cpp(f"#define UI {ui}\n#define IX {ix}\n" + harness)


if __name__ == "__main__":
    unittest.main()

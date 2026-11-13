"""Execute the LCD-only production result/wake path without display hardware."""
import unittest
import test_serial_pause_recovery as fixture


class LcdResultDimming(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_side_strip_refresh_does_not_flash_lcd_into_deep_idle(self):
        source = (fixture.ROOT / "src/leds/side_strip_handler.cpp").read_text()
        self.run_cpp(
            r'''
#include <cassert>
#include <cstdint>
#include <mutex>
enum class SideStripState {unknown,printing,active,custom_color,dimmed,idle,off};
enum class LightState {deep_idle,idle,active,printing};
unsigned light_state_shift(LightState s) {return unsigned(s)*8;}
uint8_t clamp_screen_brightness(LightState,uint8_t v) {return v;}
struct Setting {uint32_t get() const {return 0x64640a00;}};
struct {Setting screen_brightness_by_state;} config;
auto &config_store() {return config;}
uint32_t ticks_ms() {return 1000;}
int32_t ticks_diff(uint32_t a,uint32_t b) {return int32_t(a-b);}
bool printing=false;
bool print_active_for_leds() {return printing;}
''' + fixture.function(source,
                       "static LightState light_state_for_strip_state(") +
            fixture.function(
                source,
                "static LightState screen_light_state_for_strip_state(") +
            fixture.function(source,
                             "static uint8_t packed_screen_brightness(") + r'''
struct SideStripHandler {
 mutable std::mutex mutex;
 SideStripState state=SideStripState::unknown;
 LightState resolved_light_state=LightState::active;
 bool screen_forced_off=false,screen_brightness_wake_from_print_override=false;
 bool print_screen_brightness_overridden=false;
 uint32_t screen_brightness_wake_until_ms=0;
 uint8_t screen_brightness_wake_percent=100,print_screen_brightness_override=35;
 uint8_t current_screen_brightness() const;
};
''' +
            fixture.function(
                source,
                "uint8_t SideStripHandler::current_screen_brightness() const")
            + r'''
int main() {
 SideStripHandler lcd;
 assert(lcd.current_screen_brightness()==100);
 lcd.resolved_light_state=LightState::idle;
 assert(lcd.current_screen_brightness()==10);
 lcd.state=SideStripState::off; assert(lcd.current_screen_brightness()==0);
 lcd.state=SideStripState::unknown; printing=true;
 lcd.print_screen_brightness_overridden=true;
 assert(lcd.current_screen_brightness()==35);
 lcd.print_screen_brightness_override=0; assert(lcd.current_screen_brightness()==0);
 lcd.screen_forced_off=true; assert(lcd.current_screen_brightness()==0);
}
''')

    def test_retained_results_dim_and_wake_without_clearing_every_frame(self):
        source = (fixture.ROOT / "src/leds/led_manager.cpp").read_text()
        methods = "\n".join(
            fixture.function(source, signature)
            for signature in ("static bool guided_activity_active()",
                              "void LEDManager::update_lcd_brightness()",
                              "bool LEDManager::wake_lcd_from_dim_idle()"))
        self.run_cpp(r'''
#include <algorithm>
#include <array>
#include <optional>
#include <cstdint>
#include <cassert>
#define HAS_SIDE_LEDS() 0
enum class WarningType : uint8_t {NozzleTimeout,HeatersTimeout,HotendFanError};
enum class ClientFSM {Printing,Serial_printing,Warning,Workflow,_count};
namespace fsm {
struct Data {std::array<uint8_t,4> value{}; auto GetData() const {return value;}};
struct States {
 std::array<std::optional<Data>,4> values{};
 const auto &operator[](ClientFSM type) const {return values[size_t(type)];}
};
}
namespace marlin_server {
enum class State {Idle,Finished,Aborted,Exit,PrintInit,SerialPrintInit,Printing,Paused};
bool is_printing_state(State s) {return s==State::Printing;}
bool is_extended_paused_state(State s) {return s==State::Paused;}
bool serial_print_active() {return false;}
}
using marlin_server::State;
struct Vars {
 struct {State value=State::Idle; auto get() const {return value;}
 operator State() const {return value;}} print_state;
 fsm::States states;
 template<class F> auto peek_fsm_states(F f) {return f(states);}
} vars;
auto &marlin_vars() {return vars;}
struct Setting {uint32_t get() const {return 0x64640a00;}};
struct {Setting screen_brightness_by_state;} config;
auto &config_store() {return config;}
namespace TaskDeps {enum class Tasks {bootstrap_done}; bool check(Tasks) {return true;}}
uint32_t now=1000;
uint32_t ticks_ms() {return now;}
bool startup_activity_within_window(uint32_t start,uint32_t time,uint32_t duration) {return uint32_t(time-start)<duration;}
constexpr uint32_t screen_brightness_wake_ms=30000,startup_activity_duration_ms=30000;
uint8_t active_screen_brightness() {return 100;}
namespace leds {
enum class LightState {deep_idle,idle,active,printing};
unsigned light_state_shift(LightState state) {return unsigned(state)*8;}
uint8_t clamp_screen_brightness(LightState,uint8_t value) {return value;}
uint8_t minimum_screen_brightness(LightState) {return 15;}
struct LEDManager {
 bool startup_activity_active=false,print_override_session_active=false;
 bool lcd_brightness_wake_from_print_override=false,print_screen_brightness_overridden=false;
 uint32_t startup_activity_started_ms=0,lcd_brightness_wake_until_ms=0;
 uint8_t print_screen_brightness_override=100,lcd_brightness_wake_percent=100,brightness=0;
 void set_lcd_brightness(uint8_t value) {brightness=value;}
 void update_lcd_brightness();
 bool wake_lcd_from_dim_idle();
};
''' + methods + r'''
}
int main() {
 for (auto result:{State::Idle,State::Finished,State::Aborted,State::Exit}) {
  for (auto screen:{ClientFSM::Printing,ClientFSM::Serial_printing}) {
   vars.states.values={}; vars.print_state.value=State::Printing;
   vars.states.values[size_t(screen)]=fsm::Data{};
   leds::LEDManager lcd;
   now=1000; lcd.update_lcd_brightness(); assert(lcd.brightness==100);
   lcd.print_screen_brightness_overridden=true; lcd.print_screen_brightness_override=0;
   vars.print_state.value=result; lcd.update_lcd_brightness();
   assert(lcd.brightness==10 && !lcd.print_screen_brightness_overridden);
   assert(lcd.wake_lcd_from_dim_idle());
   for (auto elapsed:{0u,1000u,29999u}) {
    now=1000+elapsed; lcd.update_lcd_brightness(); assert(lcd.brightness==100);
   }
   now=31000; lcd.update_lcd_brightness(); assert(lcd.brightness==10);
   assert(vars.states.values[size_t(screen)].has_value());
   // Timeout notifications may remain visible without pinning brightness.
   for (auto warning:{WarningType::NozzleTimeout,WarningType::HeatersTimeout}) {
    vars.states.values[size_t(ClientFSM::Warning)]=fsm::Data{{uint8_t(warning)}};
    lcd.update_lcd_brightness(); assert(lcd.brightness==10);
   }
   vars.states.values[size_t(ClientFSM::Warning)]=fsm::Data{{uint8_t(WarningType::HotendFanError)}};
   lcd.update_lcd_brightness(); assert(lcd.brightness==100);
   vars.states.values[size_t(ClientFSM::Warning)].reset();
   vars.states.values[size_t(ClientFSM::Workflow)]=fsm::Data{};
   lcd.update_lcd_brightness(); assert(lcd.brightness==100);
  }
 }
}
''')


if __name__ == "__main__":
    unittest.main()

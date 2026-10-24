"""Actual LCD/encoder dispatch into production LED state/rendering, no hardware.

Touch hardware, screen widgets and the steady Printing FSM are modeled; input
handlers, side-strip activity/wake, status update and animation rendering are
production code. This cannot validate SPI electrical glitches on a real XLCD.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class LcdInputLeds(unittest.TestCase):

    def test_touch_and_encoder_do_not_change_status_pixels(self):
        gui = (ROOT / 'src/guiapi/src/gui.cpp').read_text()
        knob = (ROOT / 'src/guiapi/src/knob_event.cpp').read_text()
        side = (ROOT / 'src/leds/side_strip_handler.cpp').read_text()
        status = (ROOT / 'src/leds/status_leds_handler.cpp').read_text()
        animation_start = status.index('    using StateAnimationController')
        animation_end = status.index('} // namespace', animation_start)
        code = r'''
#include <cassert>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <array>
#include <algorithm>
#define HAS_SIDE_LEDS() 1
#define PRINTER_IS_PRUSA_XL() 0
#define PRINTER_IS_PRUSA_COREONE() 1
#define PRINTER_IS_PRUSA_COREONEL() 0
#define PRINTER_IS_PRUSA_iX() 0
#define HAS_CHAMBER_FILTRATION_API() 0
uint32_t clock_ms=1000;
uint32_t ticks_ms() {return clock_ms;}
int32_t ticks_diff(uint32_t a,uint32_t b) {return int32_t(a-b);}
template<typename T> struct Setting {T value; T get() const {return value;}};
struct Config {
 Setting<bool> run_leds{true};
 Setting<uint32_t> status_led_brightness_by_state{0x64646400};
 Setting<uint32_t> screen_brightness_by_state{0x64646400};
 Setting<uint32_t> status_led_idle_color{0},status_led_printing_color{0x0096ff},
  status_led_finished_color{0x00ff00},status_led_warning_color{0xffff00},status_led_error_color{0xff0000};
 Setting<uint32_t> status_led_finished_hold_s{30};
} config;
Config &config_store() {return config;}
#include <leds/status_leds_handler.hpp>
#include <led_animation_controller/animation_controller.hpp>
namespace marlin_server {
enum class State {Idle,PrintInit,SerialPrintInit,Printing,Paused,Finished,Aborted,Exit};
bool is_printing_state(State s) {return s==State::Printing;}
bool is_extended_paused_state(State s) {return s==State::Paused;}
bool serial_print_active() {return true;}
}
struct PrintState {marlin_server::State get() const {return marlin_server::State::Printing;}};
struct Vars {PrintState print_state;} vars;
Vars &marlin_vars() {return vars;}
namespace leds {
using namespace marlin_server;
''' + '\n'.join(
            function(status, signature)
            for signature in ('static bool post_filter_active()',
                              'static bool print_active_for_status_override()',
                              'static uint8_t packed_brightness(',
                              'static ColorRGBW raw_to_color(')
        ) + r'''
// The LCD input handlers cannot mutate the mocked steady Printing FSM.
StateAnimation marlin_to_anim_state() {return StateAnimation::Printing;}
namespace {
''' + status[animation_start:animation_end] + '\n}\n' + '\n'.join(
            function(status, signature) for signature in
            ('StateAnimationController &controller_instance()',
             'StatusLedsHandler &StatusLedsHandler::instance()',
             'void StatusLedsHandler::update()',
             'std::span<const ColorRGBW, 3> StatusLedsHandler::led_data()',
             'void StatusLedsHandler::set_idle_light_state(',
             'void StatusLedsHandler::reload_colors()',
             'void StatusLedsHandler::set_print_status_brightness(')) + r'''
enum class SideStripState {unknown,printing,active,custom_color,dimmed,idle,off};
constexpr uint32_t screen_brightness_wake_ms=30000;
bool print_active_for_leds() {return true;}
uint8_t active_screen_brightness() {return 100;}
''' + '\n'.join(
                 function(side, signature) for signature in
                 ('static LightState light_state_for_strip_state(',
                  'static LightState screen_light_state_for_strip_state(',
                  'static uint8_t packed_screen_brightness(')
             ) + r'''
struct SideStripHandler {
 mutable std::mutex mutex;
 SideStripState state=SideStripState::printing;
 LightState resolved_light_state=LightState::printing;
 bool screen_forced_off=false, startup_activity_active=false,host_idle_override=false;
 bool print_screen_brightness_overridden=false,screen_brightness_wake_from_print_override=false;
 uint8_t print_screen_brightness_override=100,screen_brightness_wake_percent=100;
 uint32_t active_timestamp_ms=1000,screen_brightness_wake_until_ms=0;
 struct Mode {bool invalidated=false; bool resume_on_activity() {bool r=invalidated;invalidated=false;return r;}} chamber_mode_state;
 struct Hold {void release_automatically() {}} rme_hold;
 static SideStripHandler &instance() {static SideStripHandler value;return value;}
 void activity_ping(); bool wake_screen_from_dim_idle();
 LightState current_light_state() const;
};
''' + '\n'.join(
                 function(side, signature) for signature in
                 ('void SideStripHandler::activity_ping()',
                  'bool SideStripHandler::wake_screen_from_dim_idle()',
                  'LightState SideStripHandler::current_light_state() const'
                  )) + r'''
}
enum class BtnState_t {Pressed,Released,Held,HeldAndRight,HeldAndLeft,HeldAndReleased};
enum class GUI_event_t {TOUCH_CLICK,TOUCH_MOVE,BTN_DN,BTN_UP,CLICK,KNOB,HELD_RELEASED};
enum class SoundType {button_echo,blind_alert,standard_alert};
namespace sound {void play(SoundType) {}}
struct Point {int x,y;};
union event_conversion_union {Point point; void *pvoid;};
struct window_t {
 int events=0;
 void WindowEvent(window_t*,GUI_event_t,void*) {++events;}
 struct Rect {bool Contain(Point) {return true;}};
 Rect get_rect_for_touch() {return {};}
};
struct Screen {
 window_t window;
 window_t *GetCapturedWindow() {return &window;}
};
struct Screens {
 Screen screen; int resets=0;
 static Screens *Access() {static Screens s;return &s;}
 Screen *Get() {return &screen;}
 void ResetTimeout() {++resets;}
 void ScreenEvent(void*,GUI_event_t,void*) {}
};
namespace marlin_client {
int clicks=0,moves=0;
void notify_server_about_knob_click() {++clicks;}
void notify_server_about_encoder_move_up() {++moves;}
void notify_server_about_encoder_move_down() {++moves;}
}
namespace gui_event {struct KnobEvent {int diff;};}
struct GuiEventContext {gui_event::KnobEvent event; bool is_accepted() {return true;}};
namespace gui::knob {
bool HeldRightAction() {return false;} bool HeldLeftAction() {return false;}
bool EventEncoder(int); bool EventClick(BtnState_t);
}
using namespace gui::knob;
''' + function(knob, 'bool gui::knob::EventEncoder(') + '\n' + function(
                      knob, 'bool gui::knob::EventClick(') + r'''
struct Jogwheel {
 int diff=0; bool pending=false; BtnState_t button=BtnState_t::Released;
 bool ConsumeButtonEvent(BtnState_t &out) {out=button;bool r=pending;pending=false;return r;}
 int ConsumeEncoderDiff() {int r=diff;diff=0;return r;}
} jogwheel;
struct TouchEvent {
 GUI_event_t type=GUI_event_t::TOUCH_CLICK; int pos_x=10,pos_y=10;
 bool present=false; explicit operator bool() const {return present;}
};
struct Touch {
 TouchEvent event;
 bool is_enabled() {return true;} void update() {}
 TouchEvent get_event() {auto r=event;event.present=false;return r;}
} touchscreen;
''' + function(gui,
               'static bool wake_screen_from_dim_idle()') + '\n' + function(
                   gui, 'void gui_handle_jogwheel()') + '\n' + function(
                       gui, 'void gui_handle_touch()') + r'''
int main() {
 auto &status=leds::StatusLedsHandler::instance();
 auto &side=leds::SideStripHandler::instance();
 status.update(); clock_ms+=1000; status.update();
 for (bool settings_refresh : {false,true}) {
 for (unsigned brightness : {100,50,0}) {
  side.print_screen_brightness_overridden=true;
  side.print_screen_brightness_override=brightness;
  side.screen_forced_off=brightness==0;
  side.screen_brightness_wake_until_ms=0;
  for (int i=0;i<1000;++i) {
   clock_ms+=5;
   status.update();
   std::array<leds::ColorRGBW,3> before;
   std::copy(status.led_data().begin(),status.led_data().end(),before.begin());
   switch(i%5) {
   case 0: touchscreen.event={GUI_event_t::TOUCH_CLICK,10,10,true};gui_handle_touch();break;
   case 1: touchscreen.event={GUI_event_t::TOUCH_MOVE,10,10,true};gui_handle_touch();break;
   case 2: jogwheel.diff=(i%2)?1:-1;gui_handle_jogwheel();break;
   case 3: jogwheel.pending=true;jogwheel.button=BtnState_t::Pressed;gui_handle_jogwheel();break;
   case 4: jogwheel.pending=true;jogwheel.button=BtnState_t::Released;gui_handle_jogwheel();break;
   }
   // Exercise refreshes that previously forced an animation restart too.
   if (settings_refresh) {status.reload_colors(); status.set_print_status_brightness(100);}
   status.set_idle_light_state(side.current_light_state());status.update();
   assert(std::equal(before.begin(),before.end(),status.led_data().begin()));
   assert(status.led_data()[1].b==255 && status.led_data()[1].g==150);
  }
 }
 }
 assert(marlin_client::clicks>500 && marlin_client::moves>500);
 assert(Screens::Access()->Get()->window.events>1500);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'freertos').mkdir()
            (path / 'freertos/mutex.hpp').write_text(
                '#pragma once\n#include <mutex>\nnamespace freertos {using Mutex=std::mutex;}\n'
            )
            (path / 'freertos/timing.hpp').write_text(
                '#pragma once\n#include <cstdint>\nextern uint32_t clock_ms;\nnamespace freertos {inline uint32_t millis() {return clock_ms;}}\n'
            )
            (path / 'test.cpp').write_text(code)
            binary = path / 'test'
            subprocess.run([
                'c++', '-std=c++23', '-fsanitize=address,undefined',
                '-fno-pie', '-no-pie', '-I' + str(path),
                '-I' + str(ROOT / 'include'),
                '-I' + str(ROOT / 'src/module/utils/buddy_utils'), '-I' +
                str(ROOT / 'src/module/leds/led_animation_controller/include'),
                str(path / 'test.cpp'),
                str(ROOT / 'src/module/utils/buddy_utils/utils/led_color.cpp'),
                '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env={
                               **os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'
                           })


if __name__ == '__main__':
    unittest.main()

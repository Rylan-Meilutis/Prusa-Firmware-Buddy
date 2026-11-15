"""Run production LCD status/read/recovery code with injected bus faults.

These tests exercise software recovery, not the physical panel or SPI wiring.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class LcdFaultRecovery(unittest.TestCase):

    def run_cpp(self, program):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "lcd"
            subprocess.run([
                "c++", "-std=c++20", "-x", "c++",
                "-fsanitize=address,undefined", "-fno-pie", "-no-pie", "-o",
                str(binary), "-"
            ],
                           input=program,
                           text=True,
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_status_reads_confirm_faults_and_preserve_brightness(self):
        source = (ROOT / "src/guiapi/src/ili9488.cpp").read_text()
        production = "\n".join(
            function(source, signature)
            for signature in ("static bool ili9488_cmd_rd(",
                              "bool ili9488_is_reset_required()",
                              "void ili9488_init(void)"))
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <deque>
#include <utility>
#include <initializer_list>
#define HAS_TOUCH() 0
#define ILI9488_FLG_SAFE 1
#define ILI9488_FLG_DMA 8
constexpr unsigned ILI9488_MAX_COMMAND_READ_LENGHT=4;
constexpr unsigned SPI_BAUDRATEPRESCALER_16=16;
constexpr uint8_t CMD_MADCTLRD=0x0b, CMD_CABCCTRL2=0xc8, DEFAULT_MADCTL=0xe0, DEFAULT_COLMOD=0x66;
constexpr int HAL_OK=0, HAL_ERROR=1;
bool display_writes_suppressed=false, reduce_display_baudrate=false, do_complete_lcd_reinit=false;
int reads=0, selected=0, resets=0, startups=0;
unsigned last_clock=0;
std::deque<std::pair<int,uint8_t>> responses;
struct Setting { bool get() {return true;} };
struct Store {Setting reduce_display_baudrate;} store;
Store &config_store() {return store;}
int spi_handle_lcd=0, ili9488_task_handle=0;
struct SPIBaudRatePrescalerGuard {
 SPIBaudRatePrescalerGuard(int,unsigned clock) {last_clock=clock;}
};
void ili9488_clr_cs() {++selected;}
void ili9488_clr_rs() {}
void ili9488_set_cs() {--selected;}
int HAL_SPI_TransmitReceive(int, uint8_t *out, uint8_t *in, unsigned size, unsigned timeout) {
 assert(out[0]==CMD_MADCTLRD && out[1]==0 && size==4 && timeout<=20);
 assert(last_clock==SPI_BAUDRATEPRESCALER_16 && selected==1 && !responses.empty());
 ++reads; auto [status,value]=responses.front(); responses.pop_front(); in[1]=value; return status;
}
struct Config {uint8_t flg=8,brightness=0;} ili9488_config;
uint8_t ili9488_flg=0;
namespace Pin {enum class State {low};}
struct {void write(Pin::State) {}} displayCs;
int osThreadGetId() {return 1;}
void debug_assert_correct_task() {}
namespace option {constexpr bool bootloader=true;}
namespace buddy::hw {
 struct Configuration {
  static Configuration &Instance() {static Configuration c;return c;}
  bool has_display_backlight_control() {return true;}
 };
}
using buddy::hw::Configuration;
void ili9488_reset() {++resets;}
void ili9488_delay_ms(int) {}
void startup_new_manufacturer() {++startups;}
void startup_old_manufacturer() {++startups;}
void ili9488_cmd_madctl(uint8_t) {}
void ili9488_cmd_colmod(uint8_t) {}
void ili9488_inversion_on() {}
void ili9488_brightness_enable() {}
void ili9488_cmd(uint8_t,const uint8_t *,unsigned) {}
void ili9488_brightness_set(uint8_t v) {ili9488_config.brightness=v;}
''' + production + r'''
int main() {
 for(uint8_t valid : {0xe0,0xf0,0xf8}) {
  responses={{HAL_OK,valid}}; int before=reads;
  assert(!ili9488_is_reset_required() && reads==before+1 && responses.empty() && selected==0);
 }
 // A one-off invalid byte or transfer failure cannot clear the whole panel.
 for(int status : {HAL_OK,HAL_ERROR}) {
  responses={{status,0},{HAL_OK,0xe0}};
  assert(!ili9488_is_reset_required() && responses.empty() && selected==0);
 }
 // Even a plausible byte is invalid if HAL reported a failed transfer.
 responses={{HAL_ERROR,0xe0},{HAL_ERROR,0xe0}};
 assert(ili9488_is_reset_required() && responses.empty() && selected==0);
 responses={{HAL_OK,0},{HAL_OK,0xff}};
 assert(ili9488_is_reset_required() && responses.empty());
 display_writes_suppressed=true; int before=reads;
 assert(!ili9488_is_reset_required() && reads==before);
 display_writes_suppressed=false;
 ili9488_init(); assert(ili9488_config.brightness==255 && resets==0);
 for(uint8_t brightness : {0,1,63,127,255}) {
  ili9488_config.brightness=brightness; do_complete_lcd_reinit=true;
  ili9488_init();
  assert(ili9488_config.brightness==brightness && !do_complete_lcd_reinit);
 }
 assert(resets==5 && startups==5);
}
''')

    def test_bare_and_normal_loops_share_error_fallback_and_full_redraw(self):
        source = (ROOT / "src/gui/display_hw_checks_xlcd.cpp").read_text()
        production = source[source.index("namespace {"):]
        counter = (
            ROOT /
            "src/module/utils/buddy_utils/utils/timing/combo_counter.hpp"
        ).read_text()
        counter = counter[counter.index("template <typename Time"):]
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <limits>
#define HAS_TOUCH() 1
#define HAS_HUMAN_INTERACTIONS() 1
unsigned now=2048;
unsigned ticks_ms() {return now;}
template<class T> struct RateLimiter {
 T interval,last=0; RateLimiter(T t):interval(t) {}
 bool check(T t) {if(last && T(t-last)<interval)return false;last=t;return true;}
};
struct Setting { bool value=false; bool get(){return value;} void set(bool v){value=v;} };
struct {Setting reduce_display_baudrate;} store;
auto &config_store() {return store;}
enum class WarningType {DisplayProblemDetected};
unsigned warnings=0,reinit=0,draw=0,logs=0;
namespace marlin_client {void set_warning(WarningType){++warnings;}}
void log_warning(int,const char*,unsigned,unsigned) {++logs;}
constexpr int GUI=0;
struct Touchscreen_GT911 {
 enum class RecoveryAction {none,restart_display};
 bool enabled=true; unsigned checks=0; RecoveryAction action=RecoveryAction::none;
 bool is_enabled(){return enabled;} void perform_check(){++checks;}
 RecoveryAction required_recovery_action(){return action;}
} touchscreen;
namespace display {
 bool failed=false; unsigned checks=0;
 bool is_reset_required(){++checks;return failed;}
 void complete_reinit(){++reinit;}
 void init(){touchscreen.action=Touchscreen_GT911::RecoveryAction::none;}
}
struct Screen {unsigned invalid=0;void Invalidate(){++invalid;}} screen;
struct Screens {
 static inline bool present=true;
 static Screens *Access(){static Screens s;return &s;}
 Screen *Get(){return present ? &screen : nullptr;}
};
void gui_invalidate(){++draw;}
namespace lcd {void communication_check();void bare_communication_check();}
''' + counter + production + r'''
int main() {
 display::failed=true;
 // Previously bare-loop errors reset forever without warning or slowdown.
 lcd::bare_communication_check(); now+=2048;
 lcd::bare_communication_check(); now+=2048;
 lcd::bare_communication_check();
 assert(reinit==3 && draw==3 && screen.invalid==3 && logs==3);
 assert(store.reduce_display_baudrate.value && warnings==1 && touchscreen.checks==0);
 unsigned checks=display::checks;
 lcd::communication_check(); assert(display::checks==checks); // shared rate limit
 now+=2048; display::failed=false;
 lcd::communication_check(); assert(reinit==3 && touchscreen.checks==1);
 touchscreen.action=Touchscreen_GT911::RecoveryAction::restart_display;
 now+=2048; lcd::communication_check();
 assert(reinit==4 && draw==4 && screen.invalid==4 && warnings==1);
 // Touch recovery is deliberately deferred in the bare loop.
 touchscreen.action=Touchscreen_GT911::RecoveryAction::restart_display;
 now+=2048; lcd::bare_communication_check(); assert(reinit==4);
 now+=2048; Screens::present=false; lcd::communication_check();
 assert(reinit==5 && draw==5 && screen.invalid==4);
}
''')

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
            (Path(directory) / "device").mkdir()
            (Path(directory) / "device/hal.h").write_text("#pragma once\n")
            (Path(directory) / "cmsis_os.h").write_text("#pragma once\n")
            subprocess.run([
                "c++", "-std=c++20", "-x", "c++", "-I", directory, "-I",
                str(ROOT / "src/guiapi/include"),
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
#define BOARD_IS_BUDDY() 0
#define ILI9488_FLG_SAFE 1
#define ILI9488_FLG_DMA 8
constexpr unsigned ILI9488_MAX_COMMAND_READ_LENGHT=4;
constexpr unsigned SPI_BAUDRATEPRESCALER_16=16;
constexpr uint8_t CMD_NOP=0, CMD_MADCTLRD=0x0b, CMD_CABCCTRL2=0xc8, DEFAULT_MADCTL=0xe0, DEFAULT_COLMOD=0x66;
constexpr int HAL_OK=0, HAL_ERROR=1;
bool display_writes_suppressed=false, reduce_display_baudrate=false, do_complete_lcd_reinit=false;
int reads=0, selected=0, resets=0, startups=0;
int edges=0, aborts=0;
bool mosi_low=false;
namespace Pin {enum class State {low,high};}
struct {
 Pin::State value=Pin::State::low;
 void write(Pin::State v) {if(v==Pin::State::high)assert(mosi_low);if(v!=value)++edges;value=v;}
} displayCs;
unsigned last_clock=0;
std::deque<std::pair<int,uint8_t>> responses;
struct Setting { bool get() {return true;} };
struct Store {Setting reduce_display_baudrate;} store;
Store &config_store() {return store;}
int spi_handle_lcd=0, ili9488_task_handle=0;
struct SPIBaudRatePrescalerGuard {
 SPIBaudRatePrescalerGuard(int,unsigned clock) {last_clock=clock;}
};
void ili9488_clr_rs() {}
// The production xBuddy CS helpers are no-ops. Model the real mux instead.
void ili9488_clr_cs() {}
void ili9488_set_cs() {}
int HAL_SPI_Abort(int) {++aborts;return HAL_OK;}
int HAL_SPI_Transmit(int,uint8_t *out,unsigned size,unsigned timeout) {
 assert(size==1 && *out==0 && timeout==20);mosi_low=true;return HAL_OK;
}
int HAL_SPI_TransmitReceive(int, uint8_t *out, uint8_t *in, unsigned size, unsigned timeout) {
 assert(out[0]==CMD_MADCTLRD && out[1]==0 && size==4 && timeout<=20);
 assert(last_clock==SPI_BAUDRATEPRESCALER_16 && displayCs.value==Pin::State::low && !responses.empty());
 // Every attempt must have its own actual high->low boundary. The old code
 // fails here even though its software CS helper calls appeared balanced.
 assert(edges==reads*4+2 && mosi_low);
 ++reads; auto [status,value]=responses.front(); responses.pop_front(); in[1]=value; return status;
}
struct Config {uint8_t flg=8,brightness=0;} ili9488_config;
struct {void reset_error(){}} spi_transfer;
uint8_t ili9488_flg=0;
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
 assert(aborts==3 && displayCs.value==Pin::State::low && edges==reads*4);
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
 bool transfer_error=false;
 bool take_transfer_error(){bool v=transfer_error;transfer_error=false;return v;}
 bool is_reset_required(){++checks;return failed;}
 void complete_reinit(){++reinit;}
 void init(){touchscreen.action=Touchscreen_GT911::RecoveryAction::none;}
}
struct Screen {unsigned invalid=0;void Invalidate(){++invalid;}} screen,menu;
struct Screens {
 static inline bool present=true;
 static inline Screen *active=&screen;
 static Screens *Access(){static Screens s;return &s;}
 Screen *Get(){return present ? active : nullptr;}
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
 // A lost frame with healthy LCD registers must not reset/blank the panel.
 touchscreen.action=Touchscreen_GT911::RecoveryAction::none;
 Screens::present=true; display::transfer_error=true; now+=2048;
 lcd::communication_check();
 assert(reinit==5 && draw==6 && screen.invalid==5 && !display::transfer_error);
 // Navigation replaces the active screen. Recover only the NEW screen's lost
 // frame; healthy screen/menu transitions must not trigger any panel reset.
 for(unsigned navigation=0;navigation<20;++navigation){
  Screens::active=(navigation%2)?&screen:&menu;
  const unsigned before=Screens::active->invalid,draw_before=draw;
  display::transfer_error=(navigation%3)==0;
  const bool dropped=display::transfer_error;
  now+=2048;lcd::communication_check();
  assert(reinit==5 && !display::transfer_error);
  assert(Screens::active->invalid==before+unsigned(dropped));
  assert(draw==draw_before+unsigned(dropped));
 }
}
''')

    def test_spi_failures_are_bounded_and_quarantine_only_the_failed_frame(
            self):
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
struct SPI_HandleTypeDef {};
using osThreadId=void*;
enum {HAL_OK,HAL_ERROR,HAL_BUSY,HAL_TIMEOUT};
enum {HAL_SPI_STATE_READY,HAL_SPI_STATE_BUSY_TX};
constexpr unsigned HAL_SPI_ERROR_NONE=0;
enum {osEventSignal,osEventTimeout};
struct osEvent {int status;struct {int32_t signals;} value;};
int start_status=HAL_OK,transfer_status=HAL_OK,state=HAL_SPI_STATE_READY;
unsigned errors=HAL_SPI_ERROR_NONE,starts=0,blocking=0,waits=0,clears=0,aborts=0;
osEvent next_event{osEventSignal,{8}};
osEvent osSignalWait(int32_t signal,uint32_t timeout){assert(signal==8);if(!timeout){++clears;return {osEventTimeout,{0}};}assert(timeout==100);++waits;return next_event;}
int HAL_SPI_Transmit(SPI_HandleTypeDef*,uint8_t*,uint16_t,uint32_t timeout){assert(timeout==100);++blocking;return transfer_status;}
int HAL_SPI_Receive(SPI_HandleTypeDef*,uint8_t*,uint16_t,uint32_t timeout){assert(timeout==100);++blocking;return transfer_status;}
int HAL_SPI_Transmit_DMA(SPI_HandleTypeDef*,uint8_t*,uint16_t){++starts;return start_status;}
int HAL_SPI_Abort(SPI_HandleTypeDef*){++aborts;return HAL_OK;}
int HAL_SPI_GetState(SPI_HandleTypeDef*){return state;}
unsigned HAL_SPI_GetError(SPI_HandleTypeDef*){return errors;}
#include <display_spi_transfer.hpp>
int main(){
 display::SpiTransfer io; SPI_HandleTypeDef spi;uint8_t buffer[16]{};
 assert(io.transmit(&spi,nullptr,8,buffer,16,true));
 assert(starts==1 && clears==1 && waits==1 && aborts==0);
 start_status=HAL_BUSY;
 assert(!io.transmit(&spi,nullptr,8,buffer,16,true));
 assert(waits==1 && aborts==1); // Never wait for a DMA that did not start.
 for(int i=0;i<100;++i)assert(!io.transmit(&spi,nullptr,8,buffer,16,true));
 assert(starts==2 && waits==1 && aborts==1);
 assert(io.take_error() && !io.take_error());
 start_status=HAL_OK;next_event.status=osEventTimeout;
 assert(!io.transmit(&spi,nullptr,8,buffer,16,true) && aborts==2);
 io.reset_error();next_event={osEventSignal,{8}};state=HAL_SPI_STATE_BUSY_TX;
 assert(!io.transmit(&spi,nullptr,8,buffer,16,true) && aborts==3); // Stale IRQ.
 io.reset_error();state=HAL_SPI_STATE_READY;errors=1;
 assert(!io.transmit(&spi,nullptr,8,buffer,16,true) && aborts==4);
 io.reset_error();errors=0;next_event={osEventSignal,{4}};
 assert(!io.transmit(&spi,nullptr,8,buffer,16,true) && aborts==5);
 io.reset_error();transfer_status=HAL_TIMEOUT;
 assert(!io.transmit(&spi,nullptr,8,buffer,1,false) && aborts==6);
 unsigned calls=blocking;
 buffer[0]=123;
 assert(!io.receive(&spi,buffer,1) && blocking==calls);
 assert(buffer[0]==0);
 io.reset_error();transfer_status=HAL_OK;
 assert(io.transmit(&spi,nullptr,8,buffer,1,false));
 assert(io.receive(&spi,buffer,1) && !io.take_error());
 transfer_status=HAL_ERROR;
 assert(!io.receive(&spi,buffer,1) && io.take_error() && aborts==7);
}
''')

    def test_mini_health_reads_are_bounded_and_accept_programmed_orientation(
            self):
        source = (ROOT / "src/guiapi/src/st7789v.cpp").read_text()
        production = "\n".join(
            function(source, signature)
            for signature in ("bool st7789v_cmd_rd(",
                              "bool st7789v_is_reset_required()"))
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <deque>
#include <utility>
#include <initializer_list>
constexpr int HAL_OK=0,HAL_ERROR=1,FLG_CS=1,FLG_RS=2;
constexpr uint8_t CMD_MADCTLRD=0x0b,ST7789V_DEF_MADCTL=0xc0;
constexpr unsigned ST7789V_MAX_COMMAND_READ_LENGHT=4;
int st7789v_flg=FLG_CS|FLG_RS,spi_handle_lcd=0,reads=0,aborts=0;
bool display_writes_suppressed=false;
std::deque<std::pair<int,uint8_t>> responses;
void st7789v_clr_cs(){st7789v_flg&=~FLG_CS;}
void st7789v_set_cs(){st7789v_flg|=FLG_CS;}
void st7789v_clr_rs(){st7789v_flg&=~FLG_RS;}
int HAL_SPI_Abort(int){++aborts;return HAL_OK;}
int HAL_SPI_TransmitReceive(int,uint8_t*out,uint8_t*in,unsigned size,unsigned timeout){
 assert(!(st7789v_flg&(FLG_CS|FLG_RS)) && out[0]==CMD_MADCTLRD && size==4 && timeout==20);
 assert(!responses.empty());++reads;auto [status,value]=responses.front();responses.pop_front();in[1]=value;return status;
}
''' + production + r'''
int main(){
 for(uint8_t orientation:{0xc0,0xe0,0xf0,0xf8}){
  responses={{HAL_OK,orientation}};assert(!st7789v_is_reset_required() && responses.empty());
 }
 responses={{HAL_ERROR,0xc0},{HAL_OK,0xc0}};assert(!st7789v_is_reset_required());
 responses={{HAL_ERROR,0xc0},{HAL_ERROR,0xc0}};assert(st7789v_is_reset_required());
 responses={{HAL_OK,0},{HAL_OK,255}};assert(st7789v_is_reset_required());
 assert(aborts==3 && (st7789v_flg&FLG_CS));
 display_writes_suppressed=true;int before=reads;assert(!st7789v_is_reset_required() && reads==before);
}
''')

    def test_touch_transactions_release_bus_before_recovery(self):
        source = (ROOT /
                  "src/hw/touchscreen/touchscreen_gt911.cpp").read_text()
        production = "\n".join(
            function(source, signature)
            for signature in ("bool Touchscreen_GT911::read_data(",
                              "bool Touchscreen_GT911::write_data("))
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
enum class HAL_StatusTypeDef {HAL_OK,HAL_ERROR};
constexpr int I2C_MEMADD_SIZE_16BIT=2,Touch=0;
int bus=0,*i2c_handle_touch=&bus,calls=0,recoveries=0;
bool locked=false,good=true;
namespace i2c {struct ChannelMutex {
 ChannelMutex(int&){assert(!locked);locked=true;}
 ~ChannelMutex(){locked=false;}
};}
HAL_StatusTypeDef transaction(unsigned timeout){assert(locked && timeout==50);++calls;return good?HAL_StatusTypeDef::HAL_OK:HAL_StatusTypeDef::HAL_ERROR;}
HAL_StatusTypeDef HAL_I2C_Mem_Read(int*,unsigned,unsigned,unsigned,uint8_t*,unsigned,unsigned timeout){return transaction(timeout);}
HAL_StatusTypeDef HAL_I2C_Mem_Write(int*,unsigned,unsigned,unsigned,uint8_t*,unsigned,unsigned timeout){return transaction(timeout);}
void log_warning(int,const char*){}void metric_record_string(int,const char*){}int metric_touch_event(){return 0;}
struct Touchscreen_GT911 {
 enum class RecoveryAction {none,restart_display};
 RecoveryAction required_recovery_action_=RecoveryAction::none;
 static constexpr unsigned i2c_device_address=0xba,read_write_timeout_ms=50;
 bool handle_read_error(){assert(!locked);i2c::ChannelMutex lock(bus);++recoveries;return true;}
 bool read_data(uint16_t,void*,uint8_t,uint8_t);
 bool write_data(uint16_t,const void*,uint8_t,uint8_t);
};
''' + production + r'''
int main(){
 Touchscreen_GT911 t;uint8_t data=0;
 assert(t.read_data(0,&data,1,1) && t.write_data(0,&data,1,1) && !locked);
 good=false;assert(!t.read_data(0,&data,1,2) && recoveries==2 && !locked);
 assert(!t.write_data(0,&data,1,2) && !locked);
 t.required_recovery_action_=Touchscreen_GT911::RecoveryAction::restart_display;
 int before=calls;assert(!t.read_data(0,&data,1,2) && !t.write_data(0,&data,1,2) && calls==before);
}
''')

    def test_transient_touch_fault_does_not_reset_a_healthy_lcd(self):
        source = (ROOT /
                  "src/hw/touchscreen/touchscreen_gt911.cpp").read_text()
        production = function(source,
                              "void Touchscreen_GT911::perform_check()")
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
enum {HAL_OK,HAL_ERROR,I2C_MEMADD_SIZE_16BIT};
int bus=0,*i2c_handle_touch=&bus,probe_status=HAL_OK,probe_count=0;
uint8_t probe_version=42,touchscreen_gt911_config[]={42};
bool mutex_held=false;
namespace i2c {struct ChannelMutex {
 ChannelMutex(int&){assert(!mutex_held);mutex_held=true;}
 ~ChannelMutex(){mutex_held=false;}
};}
int HAL_I2C_Mem_Read(int*,unsigned,unsigned,unsigned,uint8_t*p,unsigned n,unsigned timeout){
 assert(mutex_held && n==1 && timeout==50);++probe_count;*p=probe_version;return probe_status;
}
namespace screen_home_data_t {bool EverBeenOpened(){return true;}}
constexpr int Touch=0;
void log_info(int,const char*){}void log_error(int,const char*){}
void log_debug(int,const char*){}
void metric_record_string(int,const char*){}int metric_touch_event(){return 0;}
struct Touchscreen_GT911 {
 enum class RecoveryAction {none,restart_display,disable_touch};
 bool is_hw_ok_=true,read_error_recovery_queued_=true,enabled=true,ordinary_good=true;
 RecoveryAction required_recovery_action_=RecoveryAction::restart_display;
 unsigned consecutive_read_error_count_=8;
 static constexpr unsigned i2c_device_address=0xba,register_config_begin=0x8047,read_write_timeout_ms=50;
 void set_enabled(bool v){enabled=v;}
 bool read_data(unsigned,uint8_t*p,unsigned){*p=ordinary_good ? 42 : 0;return true;}
 void handle_read_error(){++consecutive_read_error_count_;}
 void perform_check();
};
''' + production + r'''
int main(){
 using T=Touchscreen_GT911;using A=T::RecoveryAction;
 T t;t.perform_check();
 assert(t.required_recovery_action_==A::none && t.consecutive_read_error_count_==0 && !t.read_error_recovery_queued_);
 assert(probe_count==1 && !mutex_held);
 // A persistent failed probe preserves the hard-reset request and budget.
 t=T{};probe_status=HAL_ERROR;t.perform_check();
 assert(t.required_recovery_action_==A::restart_display && t.consecutive_read_error_count_==8);
 t=T{};probe_status=HAL_OK;probe_version=0;t.perform_check();
 assert(t.required_recovery_action_==A::restart_display);
 // An explicit operator re-enable must still perform its full initialization.
 t=T{};t.read_error_recovery_queued_=false;unsigned before=probe_count;t.perform_check();
 assert(t.required_recovery_action_==A::restart_display && probe_count==before);
 t=T{};t.required_recovery_action_=A::none;t.perform_check();
 assert(t.consecutive_read_error_count_==0);
}
''')

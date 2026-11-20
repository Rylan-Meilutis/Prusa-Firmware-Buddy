"""Run production manual references, LCD positions and G123 guards, no hardware."""
from pathlib import Path
import unittest

import test_serial_pause_recovery as pause_tests

function = pause_tests.function

ROOT = Path(__file__).resolve().parents[3]


class ManualUnhomedMotion(unittest.TestCase):
    run_cpp = pause_tests.SerialPauseRecovery.run_cpp

    def code(self, model):
        limits = (ROOT /
                  'src/common/manual_motion_limits.hpp').read_text().replace(
                      '#include <option/has_indx.h>',
                      '').replace('#pragma once', '')
        motion = (ROOT / 'lib/Marlin/Marlin/src/module/motion.cpp').read_text()
        return r'''
#include <cassert>
#include <cmath>
#include <array>
#include <initializer_list>
#include <optional>
#include <cstdint>
#define HAS_INDX() MODEL_INDX
#define PRINTER_IS_PRUSA_XL() MODEL_REAR
#define PRINTER_IS_PRUSA_iX() 0
#define X_MIN_POS -1
#define X_MAX_POS 261
#define X_NOZZLE_CLEANER_ORIGIN 260.35f
#define Y_MIN_POS -10
#define Y_DOCK_PARKING_MIN_SAFE_POS 5
#define Y_MAX_POS 310
#define Y_MAX_PRINT_POS 210
#define Z_MIN_POS 0
#define _BV(a) (1u << (a))
#define SERIAL_ECHO_MSG(...) (++notices)
enum AxisEnum {X_AXIS,Y_AXIS,Z_AXIS,E_AXIS};
enum class AxisHomeLevel {not_homed, imprecise, full};
struct Home {
 std::array<AxisHomeLevel,3> data{};
 AxisHomeLevel &operator[](unsigned a) {return data[a];}
} axes_home_level;
uint8_t axes_need_homing(uint8_t bits,AxisHomeLevel level) {
 uint8_t result=0;
 for(unsigned a=0;a<3;++a) if((bits & _BV(a)) && axes_home_level[a]<level) result|=_BV(a);
 return result;
}
struct Pos {float x=17,y=33,z=274,e=0;} current_position,last_target;
using xyze_float_t=Pos;
uint8_t manual_assumed_axes=0;
int syncs=0, waits=0, notices=0,moves=0;
struct {void synchronize() {++waits;}} planner;
void sync_plan_position() {++syncs;}
'''.replace('MODEL_INDX', str(int(model == 'indx'))).replace(
            'MODEL_REAR',
            str(int(model == 'rear'))) + limits + '\n' + function(
                motion, 'void prepare_manual_motion_xy()') + '\n' + function(
                    motion, 'void set_axis_is_not_at_home(')

    def test_assumed_references_never_home_or_reset_z(self):
        for model in ('indx', 'rear', 'ordinary'):
            with self.subTest(model=model):
                self.run_cpp(
                    self.code(model) + r'''
int main() {
 using namespace buddy::manual_motion_safety;
 prepare_manual_motion_xy();
 assert(std::abs(current_position.x-assumed_x_position())<0.001f);
 assert(current_position.y==assumed_y_position());
 assert(current_position.z==274 && manual_assumed_axes==3 && syncs==1 && waits==1);
 assert(axes_need_homing(7,AxisHomeLevel::imprecise)==7);
 current_position.x-=1; const float moved_x=current_position.x;
 prepare_manual_motion_xy(); assert(current_position.x==moved_x && syncs==1);
 set_axis_is_not_at_home(X_AXIS); assert(manual_assumed_axes==2);
 prepare_manual_motion_xy(); assert(syncs==2 && manual_assumed_axes==3);
 axes_home_level[X_AXIS]=AxisHomeLevel::full;
 axes_home_level[Y_AXIS]=AxisHomeLevel::full;
 current_position.x=100;current_position.y=100;
 prepare_manual_motion_xy(); assert(manual_assumed_axes==0 && syncs==2);
 assert(current_position.x==100 && current_position.y==100 && current_position.z==274);
 assert(axis_move_is_safe(274,273,{0,270}));
 assert(axis_move_is_safe(274,0,{0,270}));
 assert(!axis_move_is_safe(274,275,{0,270}));
 assert(!axis_move_is_safe(274,-1,{0,270}));
 assert(!axis_move_is_safe(NAN,0,{0,270}));
}
''')

    def test_actual_g123_guard_allows_idle_unhomed_z_and_bounds_xy(self):
        source = (ROOT / 'src/marlin_stubs/G123.cpp').read_text()
        handler = function(source, 'void PrusaGcodeSuite::G123()')
        handler = handler[:handler.
                          index('    static const xyze_float_t feedrate'
                                )] + 'last_target=target_pos; ++moves;\n}\n'
        stub = r'''
namespace marlin_server {bool printing=false; bool is_printing(){return printing;} bool serial_print_active(){return false;}}
bool selected=false, cold=true;
int active_extruder=0;
struct PhysicalToolIndex {static std::optional<int> currently_selected_opt(){return selected?std::optional<int>(0):std::nullopt;}};
struct {bool tooColdToExtrude(int){return cold;}} thermalManager;
bool axis_unhomed_error(uint8_t bits,AxisHomeLevel level){return axes_need_homing(bits,level)!=0;}
unsigned get_z_max_pos_mm_rounded(){return 270;}
std::array<bool,4> present{};
std::array<float,4> requested{};
struct GCodeParser2 {
 bool parse_marlin_command(){return true;}
 unsigned index(char c){return c=='X'?0:c=='Y'?1:c=='Z'?2:3;}
 bool has_option(char c){return present[index(c)];}
 void store_option_if_present(char c,float &v){if(has_option(c))v=requested[index(c)];}
};
struct PrusaGcodeSuite {static void G123();};
'''
        main = r'''
int main(){
 using namespace buddy::manual_motion_safety;
 present[Z_AXIS]=true;requested[Z_AXIS]=273;
 PrusaGcodeSuite::G123();assert(moves==1 && last_target.z==273);
 requested[Z_AXIS]=275;PrusaGcodeSuite::G123();assert(moves==1);
 requested[Z_AXIS]=273;marlin_server::printing=true;
 PrusaGcodeSuite::G123();assert(moves==1);marlin_server::printing=false;
 present.fill(false);present[Y_AXIS]=true;
 requested[Y_AXIS]=y_range().max-1;
 PrusaGcodeSuite::G123();assert(moves==2);
 assert(axes_need_homing(7,AxisHomeLevel::imprecise)==7);
 current_position=last_target;
 present.fill(false);present[X_AXIS]=true;
 requested[X_AXIS]=x_range().min+1;
 PrusaGcodeSuite::G123();assert(moves==3);
 requested[X_AXIS]=x_range().max+1;PrusaGcodeSuite::G123();assert(moves==3);
 current_position.y=y_range().min-1; requested[X_AXIS]=current_position.x-1;
 PrusaGcodeSuite::G123();
#if HAS_INDX() || PRINTER_IS_PRUSA_XL()
 assert(moves==3);
#endif
 present.fill(false);present[E_AXIS]=true;requested[E_AXIS]=1;
 PrusaGcodeSuite::G123();assert(moves==3);
}
'''
        for model in ('indx', 'rear'):
            with self.subTest(model=model):
                self.run_cpp(self.code(model) + stub + handler + main)

    def test_lcd_positions_preserve_z_and_do_not_reset_assumed_jogs(self):
        source = (ROOT / 'src/gui/screen_menu_move.cpp').read_text()
        stub = r'''
struct Value {float value;float get(){return value;}};
struct Byte {uint8_t value=0;uint8_t get(){return value;}};
struct Vars {Byte manual_homed_axes;std::array<Value,4> native_pos{{{10},{20},{274},{0}}};} vars;
Vars &marlin_vars(){return vars;}
'''
        main = r'''
int main(){
 using namespace buddy::manual_motion_safety;
 assert(manual_menu_position(X_AXIS)==assumed_x_position());
 assert(manual_menu_position(Y_AXIS)==assumed_y_position());
 assert(manual_menu_position(Z_AXIS)==274);
 vars.manual_homed_axes.value=3<<3;
 assert(manual_menu_position(X_AXIS)==10 && manual_menu_position(Y_AXIS)==20);
 vars.manual_homed_axes.value=7;
 assert(manual_menu_position(X_AXIS)==10 && manual_menu_position(Y_AXIS)==20);
}
'''
        self.run_cpp(
            self.code('indx') + stub +
            function(source, 'static float manual_menu_position(') + main)

"""Execute actual priority-control handlers and print-fan assist arbitration."""
from pathlib import Path
import unittest
import test_serial_pause_recovery as support

ROOT = Path(__file__).resolve().parents[3]
function = support.function


class LiveControls(unittest.TestCase):
    run_cpp = support.SerialPauseRecovery.run_cpp

    def test_strict_parser_and_production_service_handler(self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        code = r'''
#include <cassert>
#include <array>
#include <optional>
#include <string>
#include <rme_live_control.hpp>
#define RME_HAS_INDX() 1
#define HAS_CHAMBER_API() 1
#define HAS_LOADCELL() 0
#define HAS_MMU2() 0
#define PRINTER_IS_PRUSA_MINI() 0
std::string message;
#define SERIAL_ECHOLNPGM(s) (message=(s))
bool lease=true,locked=false;
int writes=0,bed=0,fan=0,dock=0,flow=100,feedrate_percentage=100;
namespace serial_remote_control {bool session_active(){return lease;}}
namespace printer_lock {bool locked(){return ::locked;}}
namespace marlin_server {void set_target_bed(int value){bed=value;++writes;}}
int selected=2;
struct Tool {
 static constexpr unsigned count=8;
 unsigned index;
 static Tool from_raw(unsigned i){assert(i<count);return {i};}
 static std::optional<Tool> currently_selected_opt(){return selected>=0?std::optional<Tool>(Tool{unsigned(selected)}):std::nullopt;}
 bool is_enabled()const{return index!=7;}
 unsigned to_raw()const{return index;}
 operator unsigned()const{return index;}
};
using PhysicalToolIndex=Tool;using VirtualToolIndex=Tool;
struct Thermal {int nozzle=0;void setTargetHotend(int v,Tool){nozzle=v;++writes;}} thermalManager;
struct Planner {
 std::array<int,8> flow_percentage{};
 void refresh_e_factor(Tool t){flow=flow_percentage[t];++writes;}
} planner;
namespace buddy {
 using Temperature=float;
 namespace dock_cooldown {void request_pwm(int v){dock=v;++writes;}}
 struct Chamber {
  bool supported=true;float target=0;
  struct Caps {bool value;bool temperature_control(){return value;}bool temperature_control_at_this_moment(){return value;}};
  Caps capabilities(){return {supported};}
  void set_target_temperature(std::optional<float> t){target=t.value_or(0);++writes;}
  void set_print_fan_override(int v){fan=v;++writes;}
 } chamber_instance;
 Chamber &chamber(){return chamber_instance;}
}
''' + function(source, 'static bool handle_remote_control_service(') + r'''
void send(const char *s){assert(handle_remote_control_service(s));}
void rejected(const char *s){int before=writes;send(s);assert(writes==before);assert(message.starts_with("echo:RME_ERROR"));}
int main(){
 using rme_live_control::parse;
 for(const char *s:{"", "bed=110 fan=0", "bed=110 bed=110", "bed=110 tool=0", "bed=-1", "bed=151", "bed=110junk", "fan=256", "fan=0 tool=0", "speed=9", "speed=301", "flow=49", "flow=151", "nozzle=501", "nozzle=240 tool=2 tool=2", "tool=1", "bed=110 M112", "bed=9999999999999999999999999999", "chamber=50"}) assert(!parse(s));
 assert(parse("bed=0"));assert(parse("bed=150"));assert(parse("nozzle=500 tool=0"));
 assert(parse("chamber=0",true));assert(!parse("bed=110",true));
 send("@RME CONTROL SET bed=110");assert(bed==110&&writes==1);
 send("@RME CONTROL SET nozzle=240 tool=2");assert(thermalManager.nozzle==240);
 rejected("@RME CONTROL SET nozzle=240 tool=3");
 send("@RME CONTROL SET nozzle=0 tool=3");assert(thermalManager.nozzle==0);
 rejected("@RME CONTROL SET nozzle=240 tool=8");
 rejected("@RME CONTROL SET nozzle=240 tool=7");
 selected=-1;rejected("@RME CONTROL SET nozzle=240");selected=2;
 for(int pwm:{0,1,25,50,75,76,255}){
  std::string s="@RME CONTROL SET fan="+std::to_string(pwm);send(s.c_str());assert(fan==pwm);
 }
 send("@RME CONTROL SET dock_fan=17");assert(dock==17);
 send("@RME CONTROL SET speed=150");assert(feedrate_percentage==150);
 send("@RME CONTROL SET flow=125 tool=3");assert(flow==125);
 send("@RME TEMPERATURE SET chamber=55");assert(buddy::chamber().target==55);
 buddy::chamber().supported=false;
 rejected("@RME TEMPERATURE SET chamber=55");
 send("@RME TEMPERATURE SET chamber=0");assert(buddy::chamber().target==0);
 lease=false;rejected("@RME CONTROL SET bed=110");lease=true;
 locked=true;rejected("@RME CONTROL SET fan=0");locked=false;
 rejected("@RME CONTROL SET bed=110 fan=255");
 assert(!handle_remote_control_service("@RME FILE LIST path=/"));
}
'''
        self.run_cpp(code)
        mmu = code[:code.index('int main(){')].replace(
            '#define HAS_MMU2() 0', '#define HAS_MMU2() 1').replace(
                '#define RME_HAS_INDX() 1', '#define RME_HAS_INDX() 0'
            ).replace(
                'using PhysicalToolIndex=Tool;using VirtualToolIndex=Tool;',
                '''
struct PhysicalToolIndex:Tool {
 static constexpr unsigned count=1;
 static PhysicalToolIndex from_raw(unsigned i){assert(i==0);return {Tool{0}};}
 static std::optional<PhysicalToolIndex> currently_selected_opt(){return PhysicalToolIndex{Tool{0}};}
};using VirtualToolIndex=Tool;''')
        self.run_cpp(mmu + '''int main(){
            send("@RME CONTROL SET nozzle=215 tool=4");
            assert(writes==1 && thermalManager.nozzle==215);
            rejected("@RME CONTROL SET nozzle=215 tool=8");
            rejected("@RME CONTROL SET nozzle=215 tool=7");
        }''')
        mini = code[:code.index('int main(){')].replace(
            '#define PRINTER_IS_PRUSA_MINI() 0',
            '#define PRINTER_IS_PRUSA_MINI() 1')
        self.run_cpp(mini + '''int main(){
            send("@RME CONTROL SET bed=110");
            assert(writes==0 && message.find("unsupported")!=std::string::npos);
            assert(!handle_remote_control_service("@RME FILE LIST path=/"));
        }''')

    def test_manual_fan_can_override_heating_assist_below_thirty_percent(self):
        source = (ROOT / 'src/feature/chamber/chamber.cpp').read_text()
        start = source.index(
            '    if (should_assist && !heating_assist_active_)')
        end = source.index('\n#endif\n\n    METRIC_DEF(metric_chamber_temp',
                           start)
        step = source[start:end]
        code = r'''
#include <cassert>
#include <mutex>
#include <optional>
#include <feature/chamber/heating_policy.hpp>
#define HAS_BED_FAN() 0
#define PRINTER_IS_PRUSA_COREONE() 1
#define PRINTER_IS_PRUSA_COREONEL() 0
#define debug_assert assert
namespace marlin_server {bool is_marlin_server_thread(){return true;}}
struct Thermal {
 unsigned pwm=0;unsigned heatbreak=255;
 unsigned get_print_fan_speed(){return pwm;}
 void set_print_fan_speed(unsigned p){pwm=p;}
} thermalManager;
namespace buddy {
constexpr unsigned chamber_heating_print_fan_pwm=76;
struct Chamber {
 std::mutex mutex_;
 std::optional<float> target_temperature_=50;
 bool heating_assist_active_=false;
 unsigned heating_assist_previous_print_fan_=0,heating_assist_applied_print_fan_=0;
 std::optional<uint8_t> heating_assist_print_fan_override_;
 void set_print_fan_override(uint8_t);
 void step(bool should_assist);
};
''' + function(
            source, 'void Chamber::set_print_fan_override('
        ) + '\nvoid Chamber::step(bool should_assist){\n' + step + '\n}\n}\n' + r'''
int main(){
 buddy::Chamber c;
 c.step(true);assert(thermalManager.pwm==76);
 for(unsigned pwm:{25,0,10,75,255}){
  c.set_print_fan_override(pwm);
  for(int i=0;i<100;++i){c.step(true);assert(thermalManager.pwm==pwm);}
 }
 c.set_print_fan_override(0);c.step(true);c.step(false);
 assert(thermalManager.pwm==0);
 c.step(true);assert(thermalManager.pwm==76); // new assistance cycle default
 assert(thermalManager.heatbreak==255);
}
'''
        # This header is in src rather than src/common.
        self.run_cpp(
            code.replace(
                '#include <feature/chamber/heating_policy.hpp>', '#include "' +
                str(ROOT / 'src/feature/chamber/heating_policy.hpp') + '"'))

    def test_dispatch_and_lcd_dock_fan_paths_are_non_motion(self):
        queue = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        dispatch = function(queue, 'static bool handle_remote_service_frame(')
        self.assertIn('handle_remote_control_service(command)', dispatch)
        self.assertIn('live_controls=1', queue)
        self.assertIn('live_dock_fan=1', queue)
        gui = (ROOT / 'src/gui/MItem_print.cpp').read_text()
        click = function(gui, 'void MI_DOCKFAN::OnClick()')
        self.assertIn('marlin_client::set_dock_fan_speed', click)
        self.assertNotIn('gcode', click)
        server = (ROOT / 'src/common/marlin_server.cpp').read_text()
        self.assertIn('buddy::dock_cooldown::request_pwm(std::min', server)

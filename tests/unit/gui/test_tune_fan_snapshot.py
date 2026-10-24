"""Run production fan telemetry with and without an INDX dock fan."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class TuneFanSnapshot(unittest.TestCase):

    def test_lcd_fan_and_bed_editors_follow_remote_changes_except_while_edited(
            self):
        source = (ROOT / 'src/gui/MItem_print.cpp').read_text()
        loops = '\n'.join(
            function(source, 'void ' + name + '::Loop()')
            for name in ('MI_HEATBED', 'MI_PRINTFAN', 'MI_DOCKFAN'))
        code = r'''
#include <cassert>
#include <cstdint>
struct WiSpin {
 bool editing=false; int shown=0, loops=0;
 void Loop() {++loops;} bool is_edited() {return editing;}
 void set_value(int value) {shown=value;}
};
struct MI_HEATBED:WiSpin {void Loop();};
struct MI_PRINTFAN:WiSpin {void Loop();};
struct MI_DOCKFAN:WiSpin {void Loop();};
struct Vars {int target_bed=0, print_fan_speed=0;} vars;
Vars &marlin_vars() {return vars;}
int val_mapping(bool,int value,int from,int to) {return value*to/from;}
namespace Fans {struct Fan {int pwm=0; int get_pwm() const {return pwm;}} dock;
const Fan &dock_fan() {return dock;}}
''' + loops + r'''
int main() {
 MI_HEATBED bed; MI_PRINTFAN fan; MI_DOCKFAN dock;
 vars.target_bed=110; vars.print_fan_speed=255; Fans::dock.pwm=128;
 bed.Loop(); fan.Loop(); dock.Loop();
 assert(bed.shown==110 && fan.shown==100 && dock.shown==50);
 bed.editing=fan.editing=dock.editing=true;
 vars.target_bed=0; vars.print_fan_speed=0; Fans::dock.pwm=0;
 bed.Loop(); fan.Loop(); dock.Loop();
 assert(bed.shown==110 && fan.shown==100 && dock.shown==50);
 bed.editing=fan.editing=dock.editing=false;
 bed.Loop(); fan.Loop(); dock.Loop();
 assert(bed.shown==0 && fan.shown==0 && dock.shown==0);
 assert(bed.loops==3 && fan.loops==3 && dock.loops==3);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(code)
            subprocess.run([
                'c++', '-std=c++23', '-fsanitize=address,undefined',
                '-fno-pie', '-no-pie',
                str(path / 'test.cpp'), '-o',
                str(path / 'test')
            ],
                           check=True)
            subprocess.run([str(path / 'test')],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))

    def test_snapshot_reports_commanded_pwm_without_mutating_fans(self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        report = function(source, 'static void rme_report_fan_state()')
        self.assertIn(
            'rme_report_fan_state();',
            function(source, 'static bool handle_remote_tune_service('))
        code = r'''
#include <sstream>
#include <cassert>
#include <string>
std::ostringstream output;
#define SERIAL_ECHOPGM(x) (output << x)
#define SERIAL_ECHO(x) (output << int(x))
struct Thermal { unsigned char print_fan_speed=0; } thermalManager;
namespace Fans {
struct Fan { unsigned char pwm=0; unsigned get_pwm() const {return pwm;} } dock;
const Fan &dock_fan() {return dock;}
}
''' + report + r'''
int main() {
 for (int duty : {0,64,128,255}) {
  thermalManager.print_fan_speed=duty; Fans::dock.pwm=255-duty;
  output.str(""); output.clear(); rme_report_fan_state();
  auto expected=" fan="+std::to_string(duty)+" dock_fan=";
  #if RME_HAS_INDX()
  expected+=std::to_string(255-duty);
  #else
  expected+="-1";
  #endif
  assert(output.str()==expected);
  assert(thermalManager.print_fan_speed==duty && Fans::dock.pwm==255-duty);
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(code)
            for indx in (0, 1):
                subprocess.run([
                    'c++', '-std=c++23', '-fsanitize=address,undefined',
                    '-fno-pie', '-no-pie', f'-DRME_HAS_INDX()={indx}',
                    str(path / 'test.cpp'), '-o',
                    str(path / 'test')
                ],
                               check=True)
                subprocess.run([str(path / 'test')],
                               check=True,
                               env=dict(os.environ,
                                        ASAN_OPTIONS='detect_leaks=0'))


if __name__ == '__main__':
    unittest.main()

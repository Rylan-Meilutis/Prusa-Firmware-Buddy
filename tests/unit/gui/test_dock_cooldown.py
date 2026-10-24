"""Native clock/fan fault simulation for the actual M979 controller and parser."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class DockCooldown(unittest.TestCase):

    def test_production_controller_and_command(self):
        handler = function((ROOT / 'src/marlin_stubs/M979.cpp').read_text(),
                           'void PrusaGcodeSuite::M979()')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'Marlin/src/core').mkdir(parents=True)
            (path / 'Marlin/src/core/serial.h').write_text(r'''
#pragma once
#include <sstream>
inline std::ostringstream serial_output;
#define SERIAL_ECHOPGM(x) (serial_output << (x))
#define SERIAL_ECHO(x) (serial_output << (x))
#define SERIAL_ECHOLN(x) (serial_output << (x) << '\n')
''')
            (path / 'timing.h').write_text(r'''
#pragma once
#include <cstdint>
inline uint32_t now_ms=0;
inline uint32_t ticks_ms() {return now_ms;}
''')
            (path / 'fanctl.hpp').write_text(r'''
#pragma once
#include <cstdint>
namespace Fans {
struct Fan {
 uint16_t pwm=0; unsigned writes=0;
 uint16_t get_pwm() const {return pwm;}
 bool set_pwm(uint16_t value) {pwm=value; ++writes; return true;}
};
inline Fan dock, print, heatbreak;
inline Fan &dock_fan() {return dock;}
}
''')
            code = r'''
#include <cassert>
#include <cmath>
#include <map>
#include <limits>
#include <fanctl.hpp>
#include <timing.h>
#include <Marlin/src/core/serial.h>
#include <feature/dock_cooldown/controller.hpp>
#include <feature/dock_cooldown/policy.hpp>
struct Parser {
 std::map<char,float> values;
 bool seen(char c) {return values.contains(c);}
 bool seenval(char c) {return seen(c) && !std::isnan(values[c]);}
 float floatval(char c,float fallback=0) {return seenval(c)?values[c]:fallback;}
} parser;
namespace PrusaGcodeSuite {void M979();}
''' + handler + r'''
int main() {
 namespace cooling=buddy::dock_cooldown;
 // Default/manual mode must preserve legacy dock fan operation on park.
 Fans::dock.pwm=64; Fans::print.pwm=110; Fans::heatbreak.pwm=200;
 cooling::before_park(0); cooling::parked(0);
 assert(Fans::dock.pwm==64);
 parser.values={{'S',1},{'C',180}}; PrusaGcodeSuite::M979();
 assert(Fans::dock.pwm==255);
 cooling::request_pwm(0); assert(Fans::dock.pwm==255);
 assert(cooling::requested_pwm()==0); // calibration restores the request, not the forced PWM
 // Actual elapsed time, not number of service calls or G-code commands.
 for (int i=0;i<10000;++i) cooling::step();
 now_ms=179999; cooling::step(); assert(Fans::dock.pwm==255);
 now_ms=180000; cooling::step(); assert(Fans::dock.pwm==0);
 serial_output.str(""); cooling::report();
 assert(serial_output.str().find("remaining=0 pending_mask=0 mode=timed pwm=0")!=std::string::npos);
 // Park another nozzle: full minimum restarts from latest actual park.
 now_ms=200000; cooling::before_park(1); now_ms+=5000; cooling::parked(1);
 serial_output.str(""); cooling::report();
 assert(serial_output.str().find("remaining=180 pending_mask=2 mode=timed pwm=255")!=std::string::npos);
 // Finishing/cancel Off and calibration restoration are only requests.
 cooling::request_pwm(0); cooling::request_pwm(40);
 assert(Fans::dock.pwm==255);
 // A newer park extends the deadline past the previous park's expiry.
 now_ms=300000; cooling::before_park(0); cooling::parked(0);
 now_ms=385000; cooling::step(); assert(Fans::dock.pwm==255);
 // Picking nozzles does not shorten the elapsed-time cooldown.
 cooling::picked(1); cooling::step(); assert(Fans::dock.pwm==255);
 cooling::picked(0); cooling::step(); assert(Fans::dock.pwm==255);
 now_ms=479999; cooling::step(); assert(Fans::dock.pwm==255);
 now_ms=480000; cooling::step(); assert(Fans::dock.pwm==0);
 cooling::before_park(2); assert(Fans::dock.pwm==255);
 cooling::request_pwm(0);
 parser.values={{'S',0}}; PrusaGcodeSuite::M979(); assert(Fans::dock.pwm==0);
 cooling::request_pwm(92); cooling::step(); assert(Fans::dock.pwm==92);
 // Re-enable must not lose a still-running cooldown.
 parser.values={{'S',1},{'C',180}}; PrusaGcodeSuite::M979(); assert(Fans::dock.pwm==255);
 for (auto bad : {std::map<char,float>{{'S',2}}, {{'S',1},{'C',0}},
      {{'S',1},{'C',-1}}, {{'S',1},{'C',86401}}, {{'S',1},{'C',1.5}},
      {{'S',0},{'C',180}}, {{'C',180}}, {{'S',NAN}}}) {
  parser.values=bad; serial_output.str(""); PrusaGcodeSuite::M979();
  assert(serial_output.str().starts_with("Error:"));
  assert(Fans::dock.pwm==255);
 }
 parser.values={}; serial_output.str(""); PrusaGcodeSuite::M979();
 assert(serial_output.str().find("enabled=1")!=std::string::npos);
 assert(Fans::print.pwm==110 && Fans::heatbreak.pwm==200);
 assert(Fans::print.writes==0 && Fans::heatbreak.writes==0);
 // Millisecond wrap must respect the exact interval, without a safety hold.
 cooling::Policy policy;
 uint32_t t=0xfffffff0u;
 policy.configure(true,180,t); policy.hot_park(0,t);
 policy.request_pwm(180); // retained for S0, not an indefinite automatic On
 policy.step(t+179999u); assert(policy.pwm()==255);
 policy.step(t+180000u); assert(policy.pwm()==0 && !policy.outstanding());
 policy.configure(false,180,t+180001u); assert(policy.pwm()==180);
 policy.configure(true,5,t+180001u); policy.hot_park(1,t+180001u);
 policy.step(t+185000u); assert(policy.pwm()==255);
 policy.step(t+185001u); assert(policy.pwm()==0);
}
'''
            (path / 'test.cpp').write_text(code)
            binary = path / 'test'
            subprocess.run([
                'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                '-I' + str(path), '-I' + str(ROOT / 'src'),
                str(path / 'test.cpp'),
                str(ROOT / 'src/feature/dock_cooldown/controller.cpp'), '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env={
                               **os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'
                           })

    def test_all_dock_writers_and_park_hooks_use_policy(self):
        park = (ROOT /
                'lib/Marlin/Marlin/src/module/prusa/toolchanger_indx.cpp'
                ).read_text()
        procedure = function(park, 'bool PrusaToolChanger::park_procedure(')
        self.assertLess(procedure.index('dock_cooldown::before_park('),
                        procedure.index('stop_heating()'))
        self.assertIn('dock_cooldown::parked(',
                      function(park, 'void PrusaToolChanger::commit_park('))
        self.assertIn('dock_cooldown::picked(',
                      function(park, 'void PrusaToolChanger::commit_pickup('))
        for filename in (
                'src/common/marlin_server.cpp', 'src/marlin_stubs/M976.cpp',
                'lib/Marlin/Marlin/src/gcode/temperature/M106_M107.cpp'):
            source = (ROOT / filename).read_text()
            self.assertNotIn('Fans::dock_fan().set_pwm(', source)
            self.assertIn('dock_cooldown::request_pwm(', source)
        server = (ROOT / 'src/common/marlin_server.cpp').read_text()
        cycle = server[server.index('static void cycle()'):]
        self.assertLess(cycle.index('dock_cooldown::step();'),
                        cycle.index('if (is_cycle_running)'))


if __name__ == '__main__':
    unittest.main()

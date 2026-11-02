"""Purge lifecycle, executed-bin bounds and cause-specific runout diagnostics."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class TpuPurgeContext(unittest.TestCase):

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_executed_bin_exit_rearms_even_without_s91(self):
        source = (ROOT / 'src/common/loadcell.cpp').read_text()
        body = source.split('void Loadcell::ProcessSample(', 1)[1]
        guard = body.split('#if HAS_INDX()', 1)[1].split('#endif', 1)[0]
        code = r'''
#include <cassert>
enum { X_AXIS, Y_AXIS };
constexpr float X_WASTEBIN_SAFE_POINT=250, Y_WASTEBIN_SAFE_POINT=140;
struct Planner {
 float x=260, y=165; int reads=0;
 float get_axis_position_mm(int axis) { ++reads; return axis==X_AXIS?x:y; }
} planner;
namespace buddy::extrusion_calibration {
 bool active=false;
 bool pressure_monitor_purge_context() {return active;}
 void set_pressure_monitor_purge_context(bool value) {active=value;}
}
void sample() {
''' + guard + r'''
}
int main() {
 using namespace buddy::extrusion_calibration;
 sample(); assert(!active && planner.reads==0);
 active=true; sample(); assert(active);
 planner.x=X_WASTEBIN_SAFE_POINT; sample(); assert(!active);
 active=true; planner.x=260; planner.y=Y_WASTEBIN_SAFE_POINT;
 sample(); assert(!active);
 active=true; planner.y=165; sample(); assert(active);
 planner.y=0; sample(); assert(!active); // actual ordinary move, no S91
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'purge.cpp'
            cpp.write_text(code)
            binary = Path(directory) / 'purge'
            subprocess.run([
                'c++', '-std=c++20', '-fsanitize=address,undefined',
                str(cpp), '-o',
                str(binary)
            ],
                           check=True)
            environment = dict(os.environ,
                               ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                               UBSAN_OPTIONS='halt_on_error=1')
            subprocess.run([str(binary)], env=environment, check=True)

    def test_g12_scope_and_cleaner_guard_cover_buffered_motion(self):
        g12 = (ROOT / 'src/marlin_stubs/G12.cpp').read_text()
        entry = g12.split(
            'const bool executed = nozzle_cleaner::load_and_execute', 1)[1]
        self.assertIn('s_param == 90', entry)
        self.assertIn('s_param == 91 && executed', entry)
        self.assertLess(entry.index('planner.synchronize()'),
                        entry.index('set_pressure_monitor_purge_context'))
        cleaner = (
            ROOT /
            'src/feature/nozzle_cleaner/nozzle_cleaner.cpp').read_text()
        run = cleaner.split('bool load_and_execute(Sequence seq)',
                            1)[1].split('bool is_loader_idle()', 1)[0]
        self.assertLess(run.index('PressureMonitorSuspension'),
                        run.index('ensure_safe_cleaning'))
        self.assertLess(run.index('planner.synchronize()'),
                        run.index('return true;\n}'))

    def test_terminal_finalization_clears_purge_without_disabling_sensors(
            self):
        server = (ROOT / 'src/common/marlin_server.cpp').read_text()
        final = server.split('void static finalize_print(bool finished)', 1)[1]
        self.assertIn('set_pressure_monitor_purge_context(false)', final)
        self.assertNotIn('set_pressure_monitor_detection(false',
                         final.split('static void', 1)[0])
        sensor = (ROOT /
                  'src/feature/filament_sensor/filament_sensors_handler.cpp'
                  ).read_text()
        self.assertNotIn('pressure_monitor_purge_context', sensor)

    def test_fault_logs_distinguish_pressure_from_presence_sensor(self):
        server = (ROOT / 'src/common/marlin_server.cpp').read_text()
        self.assertIn('EXTRUSION_FAULT source=loadcell', server)
        for field in ('pressure=', 'baseline=', 'expected=', 'velocity=',
                      'forward_mm=', 'forward_seconds='):
            self.assertIn(field, server)
        m600 = (ROOT / 'src/marlin_stubs/pause/M600.cpp').read_text()
        self.assertIn('EXTRUSION_FAULT source=automatic_filament_change', m600)

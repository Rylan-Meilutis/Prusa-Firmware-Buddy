"""Run the production read-only sensor snapshot for INDX, XL and local boards."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from test_serial_pause_recovery import ROOT, function


@unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
class DashboardSensorTelemetry(unittest.TestCase):

    def test_snapshots_and_missing_hardware_do_not_invent_zero(self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        methods = '\n'.join(
            function(source, signature)
            for signature in ('static void rme_report_sensor_temperature(',
                              'static void rme_report_sensor_state('))
        code = r'''
#include <cassert>
#include <cmath>
#include <optional>
#include <sstream>
#include <array>
#include <string>
#define RME_HAS_INDX() (MODEL==1)
#define HAS_TOOLCHANGER() (MODEL==2)
#define HAS_TEMP_BOARD (MODEL!=3)
#define HAS_TEMP_HEATBREAK (MODEL==0 || MODEL==2)
#define HAS_CHAMBER_API() (MODEL!=3)
#define HAS_CPU_FAN() (MODEL==2)
std::ostringstream serial;
#define SERIAL_ECHOPGM(x) serial << x
#define SERIAL_ECHO(x) serial << (x)
#define SERIAL_CHAR(x) serial << x
#define HOTENDS 1
uint8_t active_extruder = 0;
struct PhysicalToolIndex {
    unsigned index = 0;
    operator size_t() const {return index;}
    static PhysicalToolIndex from_raw(unsigned i) {assert(i==0); return {i};}
    static std::optional<PhysicalToolIndex> currently_selected_opt();
};
std::optional<PhysicalToolIndex> selected = PhysicalToolIndex{};
auto PhysicalToolIndex::currently_selected_opt() -> std::optional<PhysicalToolIndex> {return selected;}
struct Fan {
    bool measured = true;
    unsigned get_pwm() const {return 128;}
    bool get_rpm_measured() const {return measured;}
    unsigned get_actual_rpm() const {return 3000;}
};
Fan fan;
struct Fans {
    static const Fan &heat_break(unsigned) {return fan;}
    static const Fan &print(unsigned) {return fan;}
    static const Fan &dock_fan() {return fan;}
    static const Fan &cpu() {return fan;}
};
struct Head {
    bool valid = true;
    bool get_temps_valid() const {return valid;}
    bool is_enabled() const {return valid;}
    float get_mcu_temperature() const {return 44;}
    float get_board_temperature() const {return 33;}
    float get_tpis_ambient_temperature() const {return 25.5;}
};
namespace buddy::puppies {Head indx; std::array<Head,1> dwarfs;}
namespace AdcGet {int getMCUTemp() {return 42;}}
struct Thermal {
    float degBoard() const {return 35;}
    float degHeatbreak(unsigned) const {return 50;}
} thermalManager;
namespace buddy {
struct Chamber {std::optional<float> temperature = 29; auto current_temperature() const {return temperature;}};
Chamber chamber_instance;
auto &chamber() {return chamber_instance;}
}
''' + methods + r'''
std::string snapshot() {serial.str(""); serial.clear(); rme_report_sensor_state(); return serial.str();}
int main() {
    auto value = snapshot();
    assert(value.find("buddy_mcu=42") != std::string::npos);
    assert(value.find("heatbreak_pwm=128") != std::string::npos);
    assert(value.find("heatbreak_rpm=3000") != std::string::npos);
#if MODEL==1
    assert(value.find("dock_rpm=3000") != std::string::npos);
    assert(value.find("head_ambient=25.5") != std::string::npos);
#endif
#if MODEL==1 || MODEL==2
    assert(value.find("head_mcu=44") != std::string::npos);
    assert(value.find("head_board=33") != std::string::npos);
#endif
#if MODEL==3
    assert(value.find("buddy_board=") == std::string::npos);
    assert(value.find("heatbreak_temp=") == std::string::npos);
    assert(value.find("chamber_temp=") == std::string::npos);
#endif
    fan.measured=false; value=snapshot();
    assert(value.find("heatbreak_rpm=-1") != std::string::npos);
    selected.reset(); active_extruder=HOTENDS; value=snapshot();
#if MODEL==1
    assert(value.find("heatbreak_pwm=128") != std::string::npos); // Shared head, even parked.
    buddy::puppies::indx.valid=false; value=snapshot();
    assert(value.find("head_mcu=unknown") != std::string::npos);
    assert(value.find("heatbreak_pwm=-1") != std::string::npos);
#else
    assert(value.find("heatbreak_pwm=-1") != std::string::npos);
    assert(value.find("head_mcu=") == std::string::npos); // Do not guess first XL head.
#endif
    serial.str(""); rme_report_sensor_temperature("sentinel", -1);
    rme_report_sensor_temperature("bad", NAN);
    rme_report_sensor_temperature("bad2", INFINITY);
    rme_report_sensor_temperature("zero", 0);
    assert(serial.str()==" sentinel=unknown bad=unknown bad2=unknown zero=0");
    assert(value.size() < 512);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'snapshot.cpp').write_text(code)
            for model in (0, 3):
                subprocess.run([
                    'c++', '-std=c++20', '-fsanitize=address,undefined',
                    '-fno-pie', '-no-pie',
                    '-DMODEL=%d' % model,
                    str(path / 'snapshot.cpp'), '-o',
                    str(path / 'snapshot')
                ],
                               check=True)
                subprocess.run([str(path / 'snapshot')],
                               check=True,
                               env=dict(os.environ,
                                        ASAN_OPTIONS='detect_leaks=0'))


if __name__ == '__main__':
    unittest.main()

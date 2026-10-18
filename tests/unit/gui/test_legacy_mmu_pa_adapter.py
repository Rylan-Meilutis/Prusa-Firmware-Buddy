"""Execute the legacy shared-hotend adapter against the production PA model."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class LegacyMmuPaAdapter(unittest.TestCase):
    def test_only_loaded_filament_tracks_print_temperature(self):
        source = (ROOT / "lib/Marlin/Marlin/src/module/tool_change.cpp").read_text()
        adapter = "static uint8_t pa_loaded_slot()" + source.split(
            "static uint8_t pa_loaded_slot()", 1)[1].split(
                "\n#endif\n\n#include <option/has_toolchanger.h>", 1)[0]
        stubs = r'''
#include <feature/extrusion_calibration.hpp>
#include <cassert>
#include <cmath>
#include <cstdint>
#define PRUSA_MMU2 1
#define ENABLED(x) (x)
#define SERIAL_ECHOLNPAIR(...) ((void)0)
uint8_t active_extruder=0;
namespace MMU2 {
struct Mmu {
 uint8_t slot=2;
 bool Enabled() const { return true; }
 uint8_t get_current_tool() const { return slot; }
} mmu2;
}
struct Planner {
 int synchronizations=0;
 float flow=0;
 void synchronize() { ++synchronizations; }
 bool draining() const { return false; }
 void set_max_volumetric_flow(uint8_t, float value) { flow=value; }
} planner;
namespace pressure_advance {
struct Config { float pa=0, smooth_time=.04f; } config;
Config get_axis_e_config() { return config; }
void set_axis_e_config(Config value) { config=value; }
}
'''
        main = r'''
int main() {
 using namespace buddy::extrusion_calibration;
 reset_job_results();
 for (uint8_t slot=0; slot<5; ++slot) {
   Result result{}; result.valid=true; result.confidence=.9f;
   result.pressure_reference.valid=true;
   result.pressure_reference.low_load=1;
   result.pressure_reference.high_load=20;
   result.pressure_reference.noise=.2f;
   result.max_flow_mm3_s=12;
   result.calibration_temperature=250; result.pressure_advance=.06f;
   set_job_result(slot,result);
   result.calibration_temperature=240; result.pressure_advance=.04f;
   set_job_result(slot,result);
 }
 apply_print_temperature(0,245);
 assert(job_result(2)->calibration_temperature==245);
 assert(std::abs(pressure_advance::config.pa-.05f)<.00001f);
 for (uint8_t slot : {0,1,3,4}) assert(job_result(slot)->calibration_temperature==240);
 apply_print_temperature(1,240);
 assert(job_result(2)->calibration_temperature==245);
 set_calibration_command_active(true);
 apply_print_temperature(0,240);
 assert(job_result(2)->calibration_temperature==245);
 set_calibration_command_active(false);
 MMU2::mmu2.slot=3;
 apply_selected_indx_result();
 assert(std::abs(pressure_advance::config.pa-.06f)<.00001f);
 apply_print_temperature(0,240);
 assert(std::abs(pressure_advance::config.pa-.04f)<.00001f);
 assert(job_result(2)->calibration_temperature==245);
 // An unknown/empty MMU slot must not apply another filament's PA.
 MMU2::mmu2.slot=255;
 apply_print_temperature(0,250);
 assert(std::abs(pressure_advance::config.pa-.04f)<.00001f);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "adapter.cpp").write_text(stubs + adapter + main)
            subprocess.run([
                "c++", "-std=c++20", "-I", str(ROOT / "src/common"),
                str(path / "adapter.cpp"),
                str(ROOT / "src/common/feature/extrusion_calibration.cpp"),
                "-o", str(path / "adapter")
            ], check=True)
            subprocess.run([str(path / "adapter")], check=True)

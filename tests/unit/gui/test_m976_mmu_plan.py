"""Execute the real batch validator under MMU and independent-hotend topologies."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class MmuTemperaturePlan(unittest.TestCase):

    def test_real_validator_mmu_and_indx(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        entry = "struct BatchEntry {" + source.split(
            "struct BatchEntry {", 1)[1].split("\n};", 1)[0] + "\n};\n"
        validator = "bool validate_batch(" + source.split(
            "bool validate_batch(", 1)[1].split(
                "// Resolve the live printer-side chain", 1)[0]
        stubs = r'''
#include <pa_temperature_model.hpp>
#include <m976_material.hpp>
#include <m976_indx_policy.hpp>
#include <array>
#include <cassert>
#include <cstring>
#define ENABLED(x) (x)
#define EXTRUDERS 9
#define HEATER_0_MAXTEMP 330
#define HEATER_MAXTEMP_SAFETY_MARGIN 15
namespace buddy::extrusion_calibration { constexpr size_t max_logical_filaments=8; }
struct { int extrude_min_temp=170; } thermalManager;
struct PhysicalToolIndex {
 static constexpr uint8_t count=PRUSA_MMU2 ? 1 : 8;
 uint8_t raw;
 static PhysicalToolIndex from_raw(uint8_t n) { assert(n<count); return {n}; }
 bool is_enabled() const { return raw<count; }
 uint8_t to_raw() const { return raw; }
};
struct VirtualToolIndex {
 static constexpr uint8_t count=PRUSA_MMU2 ? 5 : 8;
 uint8_t raw;
 static VirtualToolIndex from_raw(uint8_t n) { assert(n<count); return {n}; }
 bool is_enabled() const { return raw<count; }
 PhysicalToolIndex to_physical() const { return {uint8_t(PRUSA_MMU2 ? 0 : raw)}; }
};
struct Params { std::array<char,16> name{'P','L','A'}; };
struct Filament { Params parameters() const { return {}; } };
struct Config { Filament get_filament_type(uint8_t) const { return {}; } };
Config &config_store() { static Config config; return config; }
std::string_view base_material_name(const Params &) { return "PLA"; }
'''
        main = r'''
int main() {
 std::array<BatchEntry,8> entries{};
 auto entry=[](int physical, int logical) { BatchEntry e{};
   e.physical_tool=physical; e.logical_filament=logical; e.temperature=250;
   strcpy(e.material.data(),"PLA"); e.temperatures[0]=250; e.temperatures[1]=240;
   e.temperature_count=2; return e; };
 entries[0]=entry(0,0); assert(validate_batch(entries,1));
 entries[1]=entry(PRUSA_MMU2 ? 0 : 1,1); assert(validate_batch(entries,2));
 entries[1]=entry(0,0); assert(!validate_batch(entries,2));
 entries[0]=entry(0,VirtualToolIndex::count); assert(!validate_batch(entries,1));
 entries[0]=entry(PhysicalToolIndex::count,0); assert(!validate_batch(entries,1));
 entries[0]=entry(0,0); entries[0].temperatures[1]=120; assert(!validate_batch(entries,1));
 entries[0]=entry(0,0); strcpy(entries[0].material.data(),"HIPS"); assert(!validate_batch(entries,1));
 if (!PRUSA_MMU2) { entries[0]=entry(0,0); entries[1]=entry(0,1); assert(!validate_batch(entries,2)); }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cpp").write_text(stubs + entry + validator + main)
            for mmu in (0, 1):
                subprocess.run([
                    "c++", "-std=c++20",
                    "-DPRUSA_MMU2=%d" % mmu, "-I",
                    str(ROOT / "src/common"),
                    str(path / "test.cpp"), "-o",
                    str(path / "test")
                ],
                               check=True)
                subprocess.run([str(path / "test")], check=True)

    def test_mmu_force_flag_is_not_lost_by_nested_commands(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        self.assertIn('S%d P Z%.3f F%d', source)
        commands = source.split("char calibration_command[64];", 1)[1].split(
            "GcodeSuite::process_subcommands_now", 1)[0]
        for line in commands.splitlines():
            if "snprintf(calibration_command" in line:
                self.assertIn("F%d", line)

    def test_mmu_targets_only_update_the_loaded_slot(self):
        source = (ROOT /
                  "lib/Marlin/Marlin/src/module/tool_change.cpp").read_text()
        function = source.split(
            "void buddy::extrusion_calibration::apply_print_temperature(",
            1)[1].split("\n}\n", 1)[0]
        self.assertLess(function.index("currently_selected_opt() != logical"),
                        function.index("request_job_temperature"))
        mmu_change = source.split("#if HAS_MMU2()\n    match(new_tool,",
                                  1)[1].split("#elif HAS_TOOLCHANGER()", 1)[0]
        self.assertIn("apply_selected_indx_result", mmu_change)


if __name__ == "__main__":
    unittest.main()

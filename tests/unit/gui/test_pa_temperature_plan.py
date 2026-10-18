"""Execute the production manifest parser, including repeated temperatures."""
from pathlib import Path
import shutil
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class TemperaturePlan(unittest.TestCase):

    def test_real_parser(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        entry = "struct BatchEntry {" + source.split(
            "struct BatchEntry {", 1)[1].split("\n};", 1)[0] + "\n};\n"
        parser = "bool parse_uint(" + source.split(
            "bool parse_uint(", 1)[1].split("bool parse_manual_temperatures(",
                                            1)[0]
        code = r'''
#include <pa_temperature_model.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
namespace buddy::extrusion_calibration { constexpr size_t max_logical_filaments = 8; }
'''
        main = r'''
int main() {
 std::array<BatchEntry,8> entries{};
 auto parse=[&](const char *s) { entries={}; return parse_batch_manifest(s, entries); };
 assert(parse("M976 A 0:0:HIPS:250/240") == 1);
 assert(entries[0].temperature==250 && entries[0].temperature_count==2);
 assert(entries[0].temperatures[1]==240);
 assert(parse("M976 A 0:0:HIPS:240/240/240") == 1);
 assert(entries[0].temperature_count==1);
 assert(parse("M976 A 0:0:HIPS:250/230/260,1:1:ASA:260/250") == 2);
 assert(entries[0].temperature_count==3 && entries[1].temperature_count==2);
 assert(parse("M976 A 0:0:HIPS:240") == 1);
 assert(entries[0].temperature_count==1);
 for (const auto *bad : {"M976 A 0:0:HIPS:250/", "M976 A 0:0:HIPS:/250",
   "M976 A 0:0:HIPS:250//240", "M976 A 0:0:HIPS:120/250",
   "M976 A 0:0:HIPS:250/301", "M976 A 0:0:HIPS:250/24x",
   "M976 A 0:0:HIPS:230/231/232/233/234/235/236/237/238"}) assert(!parse(bad));
 assert(parse("M976 A 0:0:HIPS:230/231/232/233/234/235/236/237") == 1);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cpp").write_text(code + entry + parser + main)
            subprocess.run([
                "c++", "-std=c++20", "-fsanitize=address,undefined", "-I",
                str(ROOT / "src/common"),
                str(path / "test.cpp"), "-o",
                str(path / "test")
            ],
                           check=True)
            subprocess.run([str(path / "test")],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_internal_temperature_waits_do_not_change_print_pa(self):
        source = (ROOT /
                  "lib/Marlin/Marlin/src/gcode/temperature/M104_M109.cpp"
                  ).read_text()
        self.assertNotIn("apply_print_temperature",
                         source.split("void M109_no_parser(", 1)[1])
        self.assertEqual(source.count("apply_print_temperature("), 2)


if __name__ == "__main__":
    unittest.main()

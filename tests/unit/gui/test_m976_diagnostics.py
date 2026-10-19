"""Execute the real read-only report and bounded history without hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class AutoPaDiagnostics(unittest.TestCase):

    def test_inspection_precedes_all_calibration_side_effects(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        command = source.split("void PrusaGcodeSuite::M976()", 1)[1]
        self.assertLess(
            command.index("inspection_requested(parser.command_ptr)"),
            command.index("ScopedActiveLightHold"))
        self.assertLess(command.index("report_pa_diagnostics();"),
                        command.index("configure_confidence"))
        reset = (ROOT /
                 "src/feature/extrusion_calibration.cpp").read_text().split(
                     "void reset_job_results()", 1)[1].split("\n}", 1)[0]
        self.assertNotIn("pa_diagnostics", reset)

    @unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
    def test_history_reporting_filtering_and_early_return(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        report = "void report_pa_diagnostics()" + source.split(
            "void report_pa_diagnostics()", 1)[1].split(
                "constexpr float filament_area", 1)[0]
        code = r"""
#include <m976_diagnostics.hpp>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
using namespace buddy::m976_diagnostics;
History pa_diagnostics;
struct Parser {
 bool t = false, l = false;
 int tool = -1, slot = -1;
 bool seenval(char key) const { return key == 'T' ? t : l; }
 int intval(char key, int) const { return key == 'T' ? tool : slot; }
} parser;
std::ostringstream output;
template<class... T> void serial(const T &...t) { (output << ... << t); output << '\n'; }
#define SERIAL_ECHOLNPAIR(...) serial(__VA_ARGS__)
#define SERIAL_ECHOLN(...) serial(__VA_ARGS__)
#define HAS_INDX() 0
"""
        main = r"""
int main() {
 assert(inspection_requested("M976 I T0"));
 assert(inspection_requested("M976 L0 I*22"));
 assert(!inspection_requested("M976 A 0:0:HIPS:240"));
 assert(!inspection_requested("M976 A 0:0:HIP-00M:240"));
 assert(!inspection_requested("M976 T0 ; I"));
 static_assert(sizeof(History) < 1024);
 parser.t = true; parser.tool = 0;
 report_pa_diagnostics();
 assert(output.str().find("no_matching_attempt_this_boot") != std::string::npos);
 auto &r = pa_diagnostics.begin(0, 0, 240);
 {
  Attempt attempt(r);
  r.reason = "hotend_too_cold";
 }
 assert(std::string_view(r.outcome) == "failed");
 {
  Attempt outer(pa_diagnostics.begin(0, 0, 240));
  Attempt inner(pa_diagnostics.begin(0, 0, 240));
  r.outcome = "fallback"; r.reason = "low_snr";
  r.pa = 0.04f; r.confidence = 0.7f; r.minimum_confidence = 0.8f;
  r.cache_lookup = "manufacturer_changed"; r.cache = "not_saved";
  r.saved_cache_temperature = 240;
  r.snr = 2; r.minimum_snr = 5;
 r.elapsed_ms=20000; r.measurement_ms=18000; r.cleanup_ms=15000;
 r.excitation_mm=38.4f; r.measurements=4; r.ejections=3;
 r.search="warm_verified";
 }
 assert(std::string_view(r.outcome) == "fallback");
 pa_diagnostics.begin(7, 7, 220).outcome = "success";
 output.str(""); report_pa_diagnostics();
 assert(output.str().find("outcome=fallback reason=low_snr") != std::string::npos);
 assert(output.str().find("snr=2.000 minimum_snr=5.000") != std::string::npos);
 assert(output.str().find("cache_lookup=manufacturer_changed") != std::string::npos);
 assert(output.str().find("saved_cache_temperature=240") != std::string::npos);
 assert(output.str().find("search=warm_verified elapsed_ms=20000 measurement_ms=18000 pellet_cleanup_ms=15000") != std::string::npos);
 assert(output.str().find("excitation_mm=38.400 measurements=4 ejections=3") != std::string::npos);
 assert(output.str().find("slot=7") == std::string::npos);
 assert(std::string_view(r.outcome) == "fallback" && r.pa == 0.04f);
 parser.t = false; parser.l = true; parser.slot = 7;
 output.str(""); report_pa_diagnostics();
 assert(output.str().find("tool=7 slot=7 outcome=success") != std::string::npos);
 assert(output.str().find("slot=0") == std::string::npos);
 parser.slot = -1;
 output.str(""); report_pa_diagnostics();
 assert(output.str().find("query_error=invalid_tool_or_slot") != std::string::npos);
 assert(std::string_view(r.outcome) == "fallback");
}
"""
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            cpp.write_text(code + report + main)
            binary = Path(directory) / "test"
            subprocess.run([
                "c++", "-std=c++17", "-I",
                str(ROOT / "src/common"),
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()

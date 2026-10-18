"""Reproduce HIPS flash-cache reuse across independent host processes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class CacheSimulation(unittest.TestCase):

    def test_idle_cache_report_uses_saved_temperature_without_applying_result(
            self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        report = "void report_pa_cache(bool filter_tool" + source.split(
            "void report_pa_cache(bool filter_tool", 2)[2].split(
                "bool restore_cache(", 1)[0]
        stubs = r'''
#define path_for firmware_path_for
#include <pa_calibration_cache_storage.hpp>
#undef path_for
#include <cassert>
#include <array>
#include <sstream>
#include <string>
#include <cstring>
namespace buddy::pa_cache {
inline void path_for(char (&path)[48], uint8_t, bool=false) {
 std::strcpy(path, "cache.bin");
}
}
struct Tool {
 uint8_t to_raw() const { return 0; }
 Tool to_physical() const { return {}; }
};
struct VirtualToolIndex {
 struct Tools {
  std::array<Tool,1> skip_all_disabled() const { return {}; }
 };
 static Tools all() { return {}; }
};
struct FilamentType { static constexpr int none=0; };
struct Config {
 struct Mode { int get() const { return 1; } } auto_pa_mode;
 int get_filament_type(uint8_t) const { return 1; }
} config;
Config &config_store() { return config; }
struct Parser {
 bool s=false; int temperature=0;
 bool seenval(char) const { return s; }
 int intval(char, int) const { return temperature; }
} parser;
buddy::pa_cache::Key current;
buddy::pa_cache::Key cache_key(uint8_t, uint8_t, int16_t temperature) {
 auto key=current; key.temperature=temperature; return key;
}
std::ostringstream output;
template<class... T> void serial(const T &...t) { (output << ... << t); output << '\n'; }
#define SERIAL_ECHOLNPAIR(...) serial(__VA_ARGS__)
#define SERIAL_ECHOLN(...) serial(__VA_ARGS__)
'''
        main = r'''
int main() {
 using namespace buddy::pa_cache;
 std::strcpy(current.profile.data(), "HIP-00M");
 std::strcpy(current.material.data(), "HIPS");
 current.temperature=240; current.confidence_floor=75;
 Record saved{current, 0.046f, 12.f, 0.81f, 1.f, 20.f, 0.2f, record_version};
 assert(write_record("cache.bin", "cache.tmp", saved));
 report_pa_cache(true, 0, true, 0);
 assert(output.str().find("state=hit reason=match") != std::string::npos);
 assert(output.str().find("temperature=240 temperature_basis=saved") != std::string::npos);
 Record after{}; assert(read_record("cache.bin", after) && after==saved);
 output.str(""); parser.s=true; parser.temperature=245;
 report_pa_cache(true, 0, true, 0);
 assert(output.str().find("reason=calibration_temperature_changed") != std::string::npos);
 assert(output.str().find("temperature=245 temperature_basis=requested") != std::string::npos);
 output.str(""); parser.s=false; current.manufacturer=13;
 report_pa_cache(true, 0, true, 0);
 assert(output.str().find("reason=manufacturer_changed") != std::string::npos);
 assert(output.str().find("source=saved") != std::string::npos);
 assert(output.str().find("source=current") != std::string::npos);
 assert(read_record("cache.bin", after) && after==saved);
 output.str(""); report_pa_cache(true, 7, false, -1);
 assert(output.str().find("PA_CAL_CACHE tool=") == std::string::npos);
 output.str(""); parser.s=true; parser.temperature=-1;
 report_pa_cache(false, -1, false, -1);
 assert(output.str().find("query_error=invalid_temperature") != std::string::npos);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, binary = root / "test.cpp", root / "test"
            source.write_text(stubs + report + main)
            subprocess.run([
                "c++", "-std=c++20", "-I",
                str(ROOT / "src/common"),
                str(source), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], cwd=root, check=True)

    def test_hips_reboot_reuse_and_each_rejection(self):
        code = r'''
#include <pa_calibration_cache_storage.hpp>
#include <cassert>
#include <cstring>
#include <string_view>
using namespace buddy::pa_cache;
int main(int argc, char **argv) {
 assert(argc == 4);
 Key key{};
 std::strcpy(key.profile.data(), "HIP-00M");
 std::strcpy(key.material.data(), "HIPS");
 key.color=0xffffff; key.color_valid=1; key.manufacturer=13;
 key.nozzle=0.4f; key.temperature=240; key.profile_temperature=215;
 key.confidence_floor=75; key.minimum_snr=5;
 Record record{key, 0.046f, 12.f, 0.81f, 1.f, 20.f, 0.2f, record_version};
 if (std::string_view(argv[1]) == "save") {
  assert(write_record(argv[2], argv[3], record));
  return 0;
 }
 Record loaded{};
 const char *reason="unset";
 assert(read_record(argv[2], loaded, &reason));
 assert(reason == nullptr && loaded == record && loaded.matches(key));
 assert(loaded.rejection_reason(key) == nullptr);
 // Changing one field must identify the same rejection used by firmware.
 #define CHECK(field, value, expected) { auto next=key; next.field=value; \
 assert(std::string_view(loaded.rejection_reason(next)) == expected); \
 assert(!loaded.matches(next)); }
 CHECK(profile[0], 'X', "profile_changed");
 CHECK(material[0], 'X', "material_changed");
 CHECK(color, 0, "color_changed");
 CHECK(color_valid, 0, "color_changed");
 CHECK(manufacturer, 2, "manufacturer_changed");
 CHECK(physical_tool, 1, "physical_tool_changed");
 CHECK(nozzle, 0.6f, "nozzle_changed");
 CHECK(temperature, 245, "calibration_temperature_changed");
 CHECK(profile_temperature, 220, "profile_temperature_changed");
 CHECK(confidence_floor, 80, "confidence_floor_changed");
 CHECK(minimum_snr, 6, "minimum_snr_changed");
 CHECK(flexible, true, "flexible_policy_changed");
 #undef CHECK
 auto invalid=loaded; invalid.confidence=0.5f;
 assert(std::string_view(invalid.rejection_reason(key)) == "invalid_confidence");
 // A failed save must not destroy the preceding accepted result.
 assert(!write_record(argv[2], "/nonexistent-pa-cache-directory/cache.tmp", invalid));
 assert(read_record(argv[2], loaded) && loaded.matches(key));
 assert(!read_record(argv[3], loaded, &reason));
 assert(std::string_view(reason) == "missing_record");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, binary = root / "test.cpp", root / "test"
            source.write_text(code)
            subprocess.run([
                "c++", "-std=c++20", "-O2", "-I",
                str(ROOT / "src/common"),
                str(source), "-o",
                str(binary)
            ],
                           check=True)
            paths = [str(root / "cache.bin"), str(root / "cache.tmp")]
            subprocess.run([str(binary), "save", *paths], check=True)
            # Separate address spaces model losing all RAM on reboot.
            for _ in range(3):
                subprocess.run([str(binary), "read", *paths], check=True)


if __name__ == "__main__":
    unittest.main()

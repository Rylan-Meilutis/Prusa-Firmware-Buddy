"""Execute calibration temperature policy and RME mapping feature gates."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class CalibrationMaintenance(unittest.TestCase):

    def run_cpp(self, code, **flags):
        with tempfile.TemporaryDirectory() as directory:
            cpp, binary = Path(directory) / "test.cpp", Path(
                directory) / "test"
            cpp.write_text(code)
            subprocess.run([
                "c++", "-std=c++20", *[f"-D{k}={v}" for k, v in flags.items()],
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

    def test_calibration_owns_guard_before_all_cleanup_and_restoration(self):
        source = (
            ROOT /
            "src/feature/tool_offset_calibration/tool_offset_calibration.cpp"
        ).read_text()
        run = source.split("bool run(", 1)[1]
        self.assertLess(run.index("PressureMonitorSuspension"),
                        run.index("ScopeGuard restore_tool"))
        self.assertLess(run.index("BlockEStallDetection"),
                        run.index("G28_no_parser"))
        self.assertIn("get_tool_temperatures(tool, context)", source)
        m976 = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        self.assertIn(
            "result.calibration_temperature = record.key.temperature", m976)
        self.assertIn("flexible_calibration(slot), target_temperature", m976)
        self.assertIn('" saved_temperature="', m976)
        self.assertIn('" requested_temperature="', m976)

    def test_hips_cleaning_uses_accepted_job_temperature_only_in_print(self):
        source = (
            ROOT /
            "src/feature/tool_offset_calibration/tool_offset_calibration.cpp"
        ).read_text()
        function = "ToolTemperatures get_tool_temperatures(" + source.split(
            "ToolTemperatures get_tool_temperatures(", 1)[1].split(
                "/// Set the hotend target", 1)[0]
        stubs = r'''
#include <algorithm>
#include <optional>
#include <cstdint>
#include <cassert>
#define HAS_NOZZLE_CLEANER() 1
#define HAS_INDX() TEST_INDX
namespace tool_offset_calibration { enum class Context {Print, Calibration}; }
struct ToolTemperatures { int16_t cleaning,z_probing,xy_probing; };
constexpr int16_t DEFAULT_CLEANING_TEMP=220,DEFAULT_Z_PROBING_TEMP=150,DEFAULT_XY_PROBING_TEMP=150;
struct VirtualToolIndex { uint8_t to_raw() const { return 0; } };
struct PhysicalToolIndex {
 std::optional<VirtualToolIndex> currently_selected_virtual_tool() const { return VirtualToolIndex{}; }
};
namespace stdext {
template<class T> auto get_optional(std::optional<T> value) { return value; }
}
struct Params { int16_t nozzle_temperature=215,nozzle_preheat_temperature=175; } params;
struct FilamentType {
 FilamentType(int=1) {}
 static constexpr int none=0;
 static FilamentType for_tool_heuristic(VirtualToolIndex) { return {}; }
 Params parameters() const { return params; }
 bool operator!=(int) const { return true; }
};
namespace buddy::extrusion_calibration {
struct Result { int16_t calibration_temperature=250; } result;
bool available=true;
const Result *job_result(uint8_t) { return available ? &result : nullptr; }
}
'''
        main = r'''
int main() {
 using namespace tool_offset_calibration;
 using namespace buddy::extrusion_calibration;
 auto temps=get_tool_temperatures({},Context::Print);
 assert(temps.cleaning==(TEST_INDX ? 250 : 215));
 assert(temps.z_probing==150 && temps.xy_probing==150);
 assert(params.nozzle_temperature==215);
 assert(get_tool_temperatures({},Context::Calibration).cleaning==215);
 available=false;
 assert(get_tool_temperatures({},Context::Print).cleaning==215);
 available=true; result.calibration_temperature=0;
 assert(get_tool_temperatures({},Context::Print).cleaning==215);
 result.calibration_temperature=250; params.nozzle_temperature=260;
 assert(get_tool_temperatures({},Context::Print).cleaning==260);
}
'''
        for enabled in (0, 1):
            self.run_cpp(stubs + function + main, TEST_INDX=enabled)

    def test_toolmap_handler_uses_current_feature_flag(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        self.assertNotIn("PRUSA_TOOL_MAPPING", source)
        self.assertIn("#include <option/has_tool_mapping.h>", source)
        handler = "static bool handle_remote_toolmap_service(" + source.split(
            "static bool handle_remote_toolmap_service(", 1)[1].split(
                "static bool handle_remote_spool_join_service(", 1)[0]
        stubs = r'''
#include <string_view>
#include <string>
#include <optional>
#include <cassert>
#include <cstdlib>
#define HAS_TOOL_MAPPING() TEST_MAPPING
// PRUSA_TOOL_MAPPING is deliberately absent, as on maintained INDX/XL.
std::string output;
#define SERIAL_ECHOLNPGM(text) output+=text
struct GcodeToolIndex { static constexpr size_t count=8; };
struct VirtualToolIndex { static constexpr size_t count=8; };
struct Mapper {
 int logical=-1,physical=-1,resets=0; bool enabled=false;
 void set_mapping(int l,int p) { logical=l; physical=p; }
 void set_enable(bool value) { enabled=value; }
 void reset() { ++resets; }
} tool_mapper;
int reports=0;
void rme_report_tool_mapping() { ++reports; }
std::optional<int> remote_number(std::string_view command,const char *key) {
 auto pos=command.find(std::string(key)+"=");
 if (pos==command.npos) return {};
 return std::atoi(command.data()+pos+std::string_view(key).size()+1);
}
'''
        main = r'''
int main() {
 assert(!handle_remote_toolmap_service("@RME OTHER QUERY"));
 assert(handle_remote_toolmap_service("@RME TOOLMAP SET logical=0 physical=3"));
 assert(handle_remote_toolmap_service("@RME TOOLMAP ENABLE value=1"));
 assert(handle_remote_toolmap_service("@RME TOOLMAP QUERY"));
#if TEST_MAPPING
 assert(tool_mapper.logical==0 && tool_mapper.physical==3 && tool_mapper.enabled);
 assert(reports==1 && output.empty());
 assert(handle_remote_toolmap_service("@RME TOOLMAP SET logical=8 physical=3"));
 assert(tool_mapper.logical==0);
 assert(handle_remote_toolmap_service("@RME TOOLMAP RESET"));
 assert(tool_mapper.resets==1);
#else
 assert(tool_mapper.logical==-1 && !tool_mapper.enabled && reports==0);
 assert(output.find("unsupported feature=tool_mapping")!=output.npos);
#endif
}
'''
        for enabled in (0, 1):
            self.run_cpp(stubs + handler + main, TEST_MAPPING=enabled)


if __name__ == "__main__":
    unittest.main()

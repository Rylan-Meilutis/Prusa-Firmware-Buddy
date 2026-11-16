"""Run the production M591 handler with the actual embedded printf formatter."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class M591LoggingTests(unittest.TestCase):

    def test_temporary_and_permanent_monitor_changes_format_safely(self):
        cc, cxx = shutil.which("cc"), shutil.which("c++")
        if not cc or not cxx:
            self.skipTest("C/C++ compilers unavailable")
        root = Path(__file__).resolve().parents[3]
        source = (root / "src/marlin_stubs/M591.cpp").read_text()
        handler = source[source.index("void m591_no_parser("):source.
                         index("\n#endif // HAS_LOADCELL()")]
        stub = r'''
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include "printf.h"
extern "C" void _putchar(char) {}
enum class IsPermanent { no, yes };
enum class Restore { no, yes };
struct Setting { bool value; bool get() const { return value; } void set(bool v) { value = v; } };
struct Store { Setting stuck_filament_detection{true}, filament_movement_detection{false}; } store;
Store &config_store() { return store; }
struct Planner { unsigned syncs = 0; void synchronize() { ++syncs; } } planner;
bool effective_stuck, effective_movement;
unsigned restores;
void apply_filament_detection() { ++restores; }
struct EMotorStallDetector {
    bool enabled = false;
    static EMotorStallDetector &Instance() { static EMotorStallDetector s; return s; }
    void SetEnabled(bool v) { enabled = v; }
    void SetIgnore(uint32_t, uint32_t) {}
    bool Enabled() const { return enabled; }
};
namespace buddy::extrusion_calibration {
void set_pressure_monitor_detection(bool stuck, bool movement) {
    effective_stuck = stuck; effective_movement = movement;
}
bool pressure_monitor_has_reference() { return false; }
}
std::vector<std::string> logs;
void capture_log(const char *fmt, ...) {
    char message[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf_(message, sizeof(message), fmt, args);
    va_end(args);
    logs.emplace_back(message);
}
#define log_info(component, ...) capture_log(__VA_ARGS__)
#define SERIAL_ECHOLNPAIR_F(...) ((void)0)
#define SERIAL_ECHOLNPGM(...) ((void)0)
'''
        main = r'''
int main() {
    // S0/S1/U0/U1, each with and without permanent P. The actual formatter,
    // unlike glibc's implementation, dereferences null %s arguments.
    for (bool permanent : {false, true}) {
        for (bool enabled : {false, true}) {
            for (bool movement : {false, true}) {
                store = Store{};
                logs.clear();
                const auto stuck_arg = movement ? std::optional<bool>{} : std::optional<bool>{enabled};
                const auto movement_arg = movement ? std::optional<bool>{enabled} : std::optional<bool>{};
                m591_no_parser(stuck_arg, movement_arg,
                    permanent ? IsPermanent::yes : IsPermanent::no, Restore::no, {});
                assert(logs.size() == 1);
                const std::string expected = std::string("Filament ") + (movement ? "movement" : "stuck")
                    + " detection " + (enabled ? "on" : "off") + (permanent ? " permanent" : "");
                assert(logs[0] == expected);
                assert(store.stuck_filament_detection.value == (permanent && !movement ? enabled : true));
                assert(store.filament_movement_detection.value == (permanent && movement ? enabled : false));
#if HAS_INDX()
                assert(effective_stuck == (!movement ? enabled : true));
                assert(effective_movement == (movement ? enabled : false));
#else
                assert(EMotorStallDetector::Instance().Enabled() == ((!movement ? enabled : true) || (movement ? enabled : false)));
#endif
            }
        }
    }
    logs.clear();
    m591_no_parser(false, true, IsPermanent::no, Restore::no, {});
    assert(logs.size() == 2);
    assert(logs[0] == "Filament stuck detection off");
    assert(logs[1] == "Filament movement detection on");
    logs.clear();
    m591_no_parser({}, {}, IsPermanent::no, Restore::yes, {});
    assert(restores == 1 && logs.size() == 1);
    assert(logs[0] == "Filament detection settings restored");
    logs.clear();
    m591_no_parser({}, {}, IsPermanent::no, Restore::no, {});
    assert(logs.empty());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            formatter = work / "printf.o"
            subprocess.run([
                cc, "-std=c11", "-c",
                str(root / "lib/printf/printf.c"), "-o",
                str(formatter)
            ],
                           check=True,
                           capture_output=True)
            for indx in (0, 1):
                with self.subTest(indx=indx):
                    cpp = work / "m591.cpp"
                    binary = work / "test"
                    cpp.write_text("#define HAS_INDX() " + str(indx) + "\n" +
                                   stub + handler + main)
                    subprocess.run([
                        cxx, "-std=c++20", "-I",
                        str(root / "lib/printf"),
                        str(cpp),
                        str(formatter), "-o",
                        str(binary)
                    ],
                                   check=True,
                                   capture_output=True)
                    subprocess.run([str(binary)],
                                   check=True,
                                   capture_output=True,
                                   cwd=work)


if __name__ == "__main__":
    unittest.main()

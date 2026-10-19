"""Exercise real early-boot/error acknowledgement control flow with retained flags.

This does not emulate flash hardware or the complete MCU boot sequence.
"""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class CrashBootSelectionTests(unittest.TestCase):

    def test_crash_without_dump_can_show_error_and_then_reenter_boot(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        main = (root / "src/buddy/main.cpp").read_text()
        start = main.index("const bool want_error_screen =")
        decision = main[start:main.index(";", start) + 1]
        start = main.index("    if (want_error_screen) {")
        minimal_boot = main[start:main.index("    bsod_mark_shown();", start)]
        gui = (root / "src/gui/guimain.cpp").read_text()
        start = gui.index("    switch (crash_dump::message_get_type())")
        acknowledge = gui[start:gui.
                          index("    screen_node screen_initializer", start)]
        start = gui.index("    crash_dump::message_set_displayed();")
        acknowledge += gui[start:gui.index("\n#if HAS_LEDS()", start)]
        code = r'''
#include <cassert>
#define BUDDY_ENABLE_WUI() 0
#define BUDDY_ENABLE_CONNECT() 0
namespace crash_dump {
enum class MsgType { FACTORY_RESET, RSOD, BSOD, EMPTY };
struct Retained { bool dump_valid, dump_displayed, message_valid, message_displayed; MsgType type; } flash;
bool dump_is_valid() { return flash.dump_valid; }
bool dump_is_displayed() { return flash.dump_displayed; }
bool message_is_valid() { return flash.message_valid; }
bool message_is_displayed() { return flash.message_displayed; }
MsgType message_get_type() { return flash.type; }
void message_set_displayed() { flash.message_displayed = true; }
void dump_set_displayed() { flash.dump_displayed = true; }
void dump_set_exported() {}
}
using namespace crash_dump;
int factory_resets = 0;
struct FactoryReset { static void perform_internal() { ++factory_resets; } };
bool error_started = false, normal_started = false, heaters_disabled = false;
// Sensor worker is intentionally absent/blocked in this harness.
void hwio_safe_state() { heaters_disabled = true; }
void init_error_screen() { error_started = true; }
void boot() {
''' + decision + minimal_boot + r'''
normal_started = true;
}
void acknowledge_error() {
''' + acknowledge + r'''
}
int main() {
    for (int flags = 0; flags < 16; ++flags) {
        flash = {bool(flags & 1), bool(flags & 2), bool(flags & 4), bool(flags & 8), MsgType::BSOD};
        const bool expected = (flash.dump_valid && !flash.dump_displayed)
            || (flash.message_valid && !flash.message_displayed);
        error_started = normal_started = heaters_disabled = false;
        boot();
        assert(error_started == expected && normal_started != expected);
        if (expected) {
            assert(heaters_disabled);
            acknowledge_error();
            // New boot with the same flash contents: no reflash/factory reset.
            error_started = normal_started = false;
            boot();
            assert(normal_started && !error_started);
        }
    }
    // The only retained evidence may be the heap error message, without a dump.
    flash = {false, false, true, false, MsgType::BSOD};
    error_started = normal_started = false;
    boot(); assert(error_started && !normal_started);
    acknowledge_error(); error_started = normal_started = false;
    boot(); assert(normal_started && !error_started);
    assert(factory_resets == 0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            executable = pathlib.Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run(
                [compiler, "-std=c++17",
                 str(cpp), "-o",
                 str(executable)],
                check=True)
            subprocess.run([str(executable)], check=True, timeout=5)

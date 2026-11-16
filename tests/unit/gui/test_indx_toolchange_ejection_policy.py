"""Exercise production G12 and its opt-in INDX pellet ejection policy."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class IndxToolchangeEjectionPolicyTests(unittest.TestCase):

    def test_production_g12_scopes_only_the_second_print_ejection(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "src/marlin_stubs/G12.cpp").read_text()
        start = source.index("void PrusaGcodeSuite::G12()")
        body = source[start:source.index("\n/** @}*/", start)]
        code = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include "indx_toolchange_ejection_policy.hpp"
#define HAS_AUTO_RETRACT() 0
#define HAS_NOZZLE_CLEANER_LITE() 0
#define HAS_INDX() 1
#define HAS_WASTEBIN_FILL_TRACKING() 1
#define X_WASTEBIN_SAFE_POINT 0
#define Y_WASTEBIN_SAFE_POINT 0
int command_s, calls, pellets, skipped;
bool command_force, printing, maintenance, purge_context, fail_next, drain;
std::optional<uint8_t> selected;
struct Position { float x=100, y=100; } current_position;
struct GCodeParser2 {
    bool parse_marlin_command() { return true; }
    bool store_option_if_present(char, uint16_t &s) { s=command_s; return true; }
    template<class T> std::optional<T> option(char) { return T(command_force); }
};
struct PhysicalToolIndex {
    uint8_t raw;
    uint8_t to_raw() const { return raw; }
    static std::optional<PhysicalToolIndex> currently_selected_opt() {
        if (selected) return PhysicalToolIndex{*selected};
        return std::nullopt;
    }
};
struct Setting { bool value=false; bool get() { return value; } };
struct Config { Setting indx_entry_only_pellet_ejection; } config;
Config &config_store() { return config; }
struct Planner {
    bool draining() { return drain; }
    void synchronize() {}
} planner;
namespace marlin_server { bool is_printing() { return printing; } }
namespace buddy::extrusion_calibration {
bool calibration_command_active() { return maintenance; }
bool pressure_monitor_purge_context() { return purge_context; }
void set_pressure_monitor_purge_context(bool v) { purge_context=v; }
}
namespace nozzle_cleaner {
enum class Sequence { clean, quick_clean, deep_clean, purge_clean,
    power_panic_purge, eject_blob, enter_cleaner, exit_cleaner };
constexpr int externally_invocable_count=8;
bool load_and_execute(Sequence s) {
    if (s==Sequence::eject_blob) ++calls;
    if (fail_next || drain) { fail_next=false; return false; }
    if (s==Sequence::enter_cleaner) current_position={100,100};
    if (s==Sequence::exit_cleaner) current_position={-100,-100};
    return true;
}
}
struct WastebinWatcher {
    static WastebinWatcher &instance() { static WastebinWatcher w; return w; }
    void account_ejected_pellet() { ++pellets; }
};
#define SERIAL_ECHOLNPGM(...) (++skipped)
#define log_warning(...) ((void)0)
struct PrusaGcodeSuite { static void G12(); };
''' + body + r'''
void run(int s, bool force=false) {
    command_s=s; command_force=force; PrusaGcodeSuite::G12();
}
void reset(bool entry_only=true) {
    drain=fail_next=maintenance=false; printing=true; selected=0;
    run(91); calls=pellets=skipped=0;
    config.indx_entry_only_pellet_ejection.value=entry_only;
}
int main() {
    reset(false); // Missing/new persistent setting preserves all old ejections.
    run(90); run(1); run(30); run(1); run(30); run(91);
    assert(calls==2 && pellets==2 && skipped==0 && !purge_context);
    reset();
    run(90); run(1); run(30); run(1); run(30);
    assert(calls==1 && pellets==1 && skipped==1 && purge_context);
    run(91); assert(!purge_context && current_position.x<0);
    reset(); // Standalone/manual ejections never opt into omission.
    run(30); run(30); assert(calls==2 && skipped==0);
    reset(); // Idle entry/exit is maintenance, not a print visit.
    printing=false; run(90); run(30); run(30);
    assert(calls==2 && skipped==0);
    reset(); // Calibration cleaning remains unconditional.
    maintenance=true; run(90); run(30); run(30);
    assert(calls==2 && skipped==0);
    reset(); // Failed entry cannot arm the policy.
    fail_next=true; run(90); run(30); run(30);
    assert(calls==2 && skipped==0);
    reset(); // A failed first ejection cannot qualify a subsequent skip.
    run(90); fail_next=true; run(30); run(30);
    assert(calls==2 && pellets==1 && skipped==0);
    reset(); // Explicit recovery override always runs.
    run(90); run(30); run(30,true); run(30);
    assert(calls==3 && skipped==0);
    reset(); // Only the second request can be omitted, never arbitrary repeats.
    run(90); run(30); run(30); run(30);
    assert(calls==2 && pellets==2 && skipped==1);
    reset(); // No stale policy after exit, new entry or lost purge scope.
    run(90); run(30); run(91); run(30); assert(calls==2 && !skipped);
    reset(); run(90); run(30); run(90); run(30); assert(calls==2 && !skipped);
    reset(); run(90); run(30); purge_context=false; run(30);
    assert(calls==2 && !skipped);
    reset(); // Cancelled/maintenance/tool-less/tool-changed visits fail safe.
    run(90); run(30); printing=false; run(30); assert(calls==2 && !skipped);
    reset(); run(90); run(30); maintenance=true; run(30); assert(calls==2 && !skipped);
    reset(); run(90); run(30); selected=1; run(30); assert(calls==2 && !skipped);
    reset(); run(90); run(30); selected.reset(); run(30); assert(calls==2 && !skipped);
    reset(); // Moving outside the bin cannot suppress the native safe-entry path.
    run(90); run(30); current_position={-100,-100}; run(30);
    assert(calls==2 && !skipped);
    reset(); // Other maintenance sequences or a failed wipe invalidate the visit.
    run(90); run(30); run(20); run(30); assert(calls==2 && !skipped);
    reset(); run(90); run(30); fail_next=true; run(1); run(30);
    assert(calls==2 && !skipped);
    reset(); // Draining must not be bypassed by the skip path.
    run(90); run(30); drain=true; run(30);
    assert(calls==2 && pellets==1 && !skipped);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            executable = Path(directory) / "test"
            cpp.write_text(code)
            build = subprocess.run([
                compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I",
                str(ROOT / "src/common"),
                str(cpp), "-o",
                str(executable)
            ],
                                   capture_output=True,
                                   text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            subprocess.run([str(executable)], check=True, capture_output=True)

    def test_option_defaults_to_both_and_is_visible_only_on_indx(self):
        store = (
            ROOT /
            "src/persistent_stores/store_instances/config_store/store_definition.hpp"
        ).read_text()
        self.assertIn(
            'StoreItem<bool, false, ItemFlag::features, journal::hash("INDX entry only pellet eject")>',
            store)
        menu = (ROOT / "src/gui/screen_menu_hardware.hpp").read_text()
        self.assertIn('#if HAS_INDX()\n    MI_INDX_PELLET_EJECTION,\n#endif',
                      menu)
        impl = (ROOT / "src/gui/MItem_hardware.cpp").read_text()
        self.assertIn('N_("Entry + Exit")', impl)
        self.assertIn('N_("Entry Only")', impl)
        self.assertIn(
            'indx_entry_only_pellet_ejection.set(current_item() == 1)', impl)

    def test_direct_maintenance_and_native_exit_do_not_use_the_policy(self):
        cleaner = (
            ROOT /
            "src/feature/nozzle_cleaner/nozzle_cleaner.cpp").read_text()
        self.assertNotIn("indx_entry_only_pellet_ejection", cleaner)
        self.assertNotIn("skip_exit", cleaner)
        exit_sequence = cleaner.split(
            '{ Sequence::exit_cleaner, {',
            1)[1].split('{ Sequence::enter_cleaner_from_inside, {', 1)[0]
        self.assertIn('G750 Y99.5 F21000 A', exit_sequence)
        self.assertIn('G750 X-12 F21000 A', exit_sequence)
        self.assertNotIn('eject_blob', exit_sequence)


if __name__ == "__main__":
    unittest.main()

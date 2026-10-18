"""Run the production confirmation handler with hardware-independent stubs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class RunoutConfirmation(unittest.TestCase):

    def test_recovery_emits_terminal_parent_event(self):
        text = (ROOT / "src/marlin_stubs/pause/M600.cpp").read_text()
        handler = text.split("void PrusaGcodeSuite::M1601()", 1)[1]
        self.assertIn("SerialPrinting::notify_workflow(recovery_workflow",
                      handler)
        self.assertIn('"canceled" : "closed"', handler)

    def test_fsm_holder_preserves_specific_fault_mode(self):
        text = (ROOT / "src/marlin_stubs/pause/pause.cpp").read_text()
        getter = text.split("LoadUnloadMode Pause::get_load_unload_mode()",
                            1)[1].split("bool Pause::should_park", 1)[0]
        self.assertIn("get_mode().value_or(LoadUnloadMode::FilamentStuck)",
                      getter)

    def test_indx_has_recovery_error_descriptor(self):
        text = (
            ROOT /
            "lib/Prusa-Error-Codes/yaml/buddy-error-codes.yaml").read_text()
        error = text.split('code: "XX101"', 1)[1].split('code:', 1)[0]
        self.assertIn("COREONE_INDX", error)
        self.assertIn("COREONEL_INDX", error)
        for variant in ("COREONE_INDX", "COREONEL_INDX"):
            assets = (
                ROOT /
                ("src/gui/res/" + variant + "_used_imgs.txt")).read_text()
            self.assertIn("error_48x48.png", assets.splitlines())

    def test_indx_without_extruder_sensor_keeps_all_prompt_layers(self):
        paths = [
            "src/marlin_stubs/include/pause_stubbed.hpp",
            "src/marlin_stubs/pause/pause.cpp",
            "src/gui/screen/screen_filament_change.cpp",
            "src/common/marlin_server_types/fsm/filament_change_phases.hpp",
            "src/common/marlin_server_types/fsm/filament_change_phases.cpp",
            "src/state/printer_state.cpp",
            "lib/Marlin/Marlin/src/gcode/queue.cpp",
        ]
        for path in paths:
            with self.subTest(path=path):
                text = (ROOT / path).read_text()
                self.assertNotIn("HAS_LOADCELL() && HAS_EXTRUDER_FSENSOR()",
                                 text)
                self.assertIn("#if HAS_LOADCELL()", text)

    @unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
    def test_confirmation_wait_continue_unload_abort(self):
        text = (ROOT / "src/marlin_stubs/pause/pause.cpp").read_text()
        handler = "void Pause::filament_stuck_ask_process" + text.split(
            "void Pause::filament_stuck_ask_process", 1)[1].split(
                "\n#endif", 1)[0]
        code = r"""
#include <cassert>
#include <optional>
#include <stdexcept>
enum class Response { _none, Continue, Unload, Abort };
enum class LoadUnloadMode { FilamentStuck, FilamentRunout, FilamentNotMoving, FlowLimit };
enum class PhasesLoadUnload { initial, FilamentStuck };
enum class LoadState { asking, unload_wait_temp, _finished, _stopped };
bool aborted = false;
int reports = 0;
namespace marlin_server { void print_abort() { aborted = true; } }
namespace SerialPrinting {
 void notify_error(const char *, const char *, const char *) { ++reports; }
}
void bsod_unreachable() { throw std::logic_error("unexpected response"); }
struct Pause {
 PhasesLoadUnload phase = PhasesLoadUnload::initial;
 LoadState state = LoadState::asking;
 LoadUnloadMode mode = LoadUnloadMode::FilamentRunout;
 auto getPhase() { return phase; }
 void setPhase(PhasesLoadUnload p) { phase = p; }
 std::optional<LoadUnloadMode> get_mode() { return mode; }
 void set(LoadState s) { state = s; }
 void filament_stuck_ask_process(Response);
};
"""
        main = r"""
int main() {
 for (auto mode : {LoadUnloadMode::FilamentStuck, LoadUnloadMode::FilamentRunout,
                   LoadUnloadMode::FilamentNotMoving, LoadUnloadMode::FlowLimit}) {
  Pause p; p.mode = mode;
  int previous_reports = reports;
  for (int i=0; i<100; ++i) p.filament_stuck_ask_process(Response::_none);
  assert(p.state == LoadState::asking && reports == previous_reports + 1);
  p.filament_stuck_ask_process(Response::Continue);
  assert(p.state == LoadState::_finished);
  p.filament_stuck_ask_process(Response::Unload);
  assert(p.state == LoadState::unload_wait_temp);
  aborted = false;
  p.filament_stuck_ask_process(Response::Abort);
  assert(aborted && p.state == LoadState::_stopped);
 }
}
"""
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            cpp.write_text(code + handler + main)
            binary = Path(directory) / "test"
            subprocess.run(["c++", "-std=c++17",
                            str(cpp), "-o",
                            str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()

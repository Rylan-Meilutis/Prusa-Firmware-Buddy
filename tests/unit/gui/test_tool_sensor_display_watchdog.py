"""Run the real GUI-side observer while the simulated puppy worker is blocked."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class ToolSensorDisplayWatchdogTests(unittest.TestCase):

    def test_blocked_worker_is_bounded_independently(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        worker = (root / "src/puppies/puppy_task.cpp").read_text()
        worker = worker[worker.index(
            "[[nodiscard]] static bool wait_for_tool_offset_sensor("):]
        self.assertLess(
            worker.index(
                "bootstrap_state_set(0, BootstrapStage::tool_offset_sensor_unknown);"
            ), worker.index("cyphal_bridge_host().refresh(bus)"))
        source = (root / "src/gui/screen_splash.cpp").read_text()
        start = source.index("static void check_tool_sensor_bootstrap(")
        end = source.index("\n#endif", start)
        self.assertIn(
            "check_tool_sensor_bootstrap(buddy::bootstrap_state_get());",
            source[end:])
        gui = (root / "src/gui/guimain.cpp").read_text()
        loop = gui[gui.index(
            "while (!TaskDeps::check(TaskDeps::Tasks::bootstrap_done))"):]
        self.assertLess(
            loop.index("ScreenSplash::check_bootstrap_progress();"),
            loop.index("gui_bare_loop();"))
        code = r'''
#include <cassert>
#include <optional>
#include <stdexcept>
#include "tool_sensor_bootstrap_policy.hpp"
namespace buddy {
enum class BootstrapStage { initial, tool_offset_sensor_unknown, tool_offset_sensor_verify, tool_offset_sensor_flash, tool_offset_sensor_ready };
struct BootstrapState { BootstrapStage stage; uint8_t percent; };
}
using buddy::BootstrapStage;
enum class ErrCode { ERR_SYSTEM_PUPPY_RUN_TIMEOUT };
uint32_t clock_ms = 0;
uint32_t ticks_ms() { return clock_ms; }
void fatal_error(ErrCode) { throw std::runtime_error("blocked sensor startup"); }
''' + source[start:end] + r'''
int main() {
    auto poll = [&](uint32_t now, BootstrapStage stage, uint8_t progress = 0) {
        clock_ms = now; check_tool_sensor_bootstrap({stage, progress});
    };
    auto must_fail = [&](uint32_t now, BootstrapStage stage, uint8_t progress = 0) {
        bool failed = false;
        try { poll(now, stage, progress); } catch (const std::runtime_error &) { failed = true; }
        assert(failed);
        poll(now, BootstrapStage::initial); // New boot/reset clears observer.
    };
    // No puppy code executes between these polls: its refresh is stuck.
    poll(0, BootstrapStage::tool_offset_sensor_verify);
    poll(29999, BootstrapStage::tool_offset_sensor_verify);
    must_fail(30000, BootstrapStage::tool_offset_sensor_verify);
    // Flashing gets a longer budget; repeated same percentage is not progress.
    poll(40000, BootstrapStage::tool_offset_sensor_flash, 10);
    poll(159999, BootstrapStage::tool_offset_sensor_flash, 10);
    must_fail(160000, BootstrapStage::tool_offset_sensor_flash, 10);
    // Real progress extends the stall deadline, but never the absolute ceiling.
    poll(200000, BootstrapStage::tool_offset_sensor_flash, 0);
    for (uint32_t now = 201000; now < 560000; now += 1000) {
        poll(now, BootstrapStage::tool_offset_sensor_flash, uint8_t((now - 200000) / 4000));
    }
    must_fail(560000, BootstrapStage::tool_offset_sensor_flash, 90);
    // Readiness and unrelated boot stages disarm it. No persistent latch.
    poll(600000, BootstrapStage::tool_offset_sensor_verify);
    poll(620000, BootstrapStage::tool_offset_sensor_ready);
    poll(900000, BootstrapStage::initial);
    poll(UINT32_MAX - 10000, BootstrapStage::tool_offset_sensor_verify);
    poll(19998, BootstrapStage::tool_offset_sensor_verify);
    must_fail(19999, BootstrapStage::tool_offset_sensor_verify);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            executable = pathlib.Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run([
                compiler, "-std=c++17", "-I",
                str(root / "src/common"),
                str(cpp), "-o",
                str(executable)
            ],
                           check=True)
            subprocess.run([str(executable)], check=True, timeout=5)

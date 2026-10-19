"""Execute the production boot loop with silent/absent/faulted sensor models."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class ToolSensorBootRecoveryTests(unittest.TestCase):

    def test_required_sensor_recovers_or_reports_error_without_being_skipped(
            self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/puppies/puppy_task.cpp").read_text()
        start = source.index(
            "[[nodiscard]] static bool wait_for_tool_offset_sensor(")
        end = source.index("\n#endif", start)
        function = source[start:end]
        code = r'''
#include <cassert>
#include <stdexcept>
#include "tool_sensor_bootstrap_policy.hpp"
#define HAS_XBUDDY_EXTENSION() 1
#define HAS_XL_CAN() 0
#define log_error(...) ((void)0)
using buddy::puppies::ToolSensorBootstrapDeadline;
namespace xbuddy_extension { enum class NodeState { unknown, verify, flash, ready }; }
enum class CommunicationStatus { OK, ERROR };
struct PuppyModbus {};
enum class BootstrapStage { tool_offset_sensor_unknown, tool_offset_sensor_verify, tool_offset_sensor_flash, tool_offset_sensor_ready };
static uint32_t clock_ms = 0;
static int error_source = 0;
static bool xbe_requires_reset = false;
static bool enabled = false;
static auto state = xbuddy_extension::NodeState::verify;
uint32_t ticks_ms() { return clock_ms; }
int32_t ticks_diff(uint32_t a, uint32_t b) { return int32_t(a - b); }
void bootstrap_state_set(int, BootstrapStage) {}
void puppy_run_timeout() { throw std::runtime_error("sensor startup error"); }
struct Bridge {
    CommunicationStatus refresh(PuppyModbus &) {
        clock_ms += 1000;
        return error_source == 1 ? CommunicationStatus::ERROR : CommunicationStatus::OK;
    }
    uint8_t get_flash_progress_percent() { return 10; }
} bridge;
Bridge &cyphal_bridge_host() { return bridge; }
struct Sensor {
    CommunicationStatus refresh(PuppyModbus &) { return error_source == 2 ? CommunicationStatus::ERROR : CommunicationStatus::OK; }
    auto get_node_state() { return state; }
    void set_enabled(bool value) { enabled = value; }
} tool_offset_sensor;
''' + function + r'''
int main() {
    PuppyModbus bus;
    auto assert_failure = [&] {
        xbe_requires_reset = false;
        assert(!wait_for_tool_offset_sensor(bus));
        assert(xbe_requires_reset);
        assert(!enabled);
    };
    auto assert_terminal = [&] {
        bool reported = false;
        try { (void)wait_for_tool_offset_sensor(bus); }
        catch (const std::runtime_error &) { reported = true; }
        assert(reported && !enabled);
    };
    auto successful_boot = [&] {
        state = xbuddy_extension::NodeState::ready;
        error_source = 0;
        assert(wait_for_tool_offset_sensor(bus));
        assert(enabled);
        enabled = false;
    };
    assert_failure(); assert_failure(); assert_terminal(); // Silent verifying node.
    successful_boot(); // Successful boot resets the consecutive failure count.
    state = xbuddy_extension::NodeState::flash;
    assert_failure(); assert_failure(); assert_terminal(); // Stalled flashing.
    successful_boot(); state = xbuddy_extension::NodeState::unknown;
    assert_failure(); assert_failure(); assert_terminal(); // Absent required node.
    for (int fault : {1, 2}) {
        successful_boot(); error_source = fault;
        assert_failure(); assert_failure(); assert_terminal(); // Modbus failure.
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run([
                compiler, "-std=c++17", "-I",
                str(root / "src/common"),
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True, timeout=5)

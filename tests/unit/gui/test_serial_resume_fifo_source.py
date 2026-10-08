"""Guard queued host moves while INDX pause recovery owns the tool."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class SerialResumeFifoTests(unittest.TestCase):

    def test_pause_docks_before_accessible_park_and_resume_primes_after_pickup(
            self):
        source = (ROOT / "src/common/marlin_server.cpp").read_text()
        pause = source.split("case State::Pausing_ParkHead:",
                             1)[1].split("case State::Paused:", 1)[0]
        self.assertLess(pause.index("tool_change(NoTool"),
                        pause.index("sync_plan_position_e()"))
        self.assertLess(pause.index("sync_plan_position_e()"),
                        pause.index("mapi::park("))
        self.assertIn("X_BED_SIZE - 8.0f", pause)
        self.assertIn("Y_BED_SIZE - 0.5f", pause)
        self.assertNotIn("server.resume.pos =", pause)
        resume = source.split("void resuming_begin(void) {",
                              1)[1].split("const GCodeReaderStreamRestoreInfo",
                                          1)[0]
        self.assertLess(resume.index("tool_change(PhysicalToolIndex"),
                        resume.index("thermalManager.setTargetHotend"))
        recovery = source.split(
            "case State::Resuming_ExecutingGCodeInterrupt:",
            1)[1].split("case State::Resuming_UnparkHead_ZE:", 1)[0]
        self.assertLess(recovery.index("unpark_prime()"),
                        recovery.index("unpark_head_XY()"))
        self.assertLess(recovery.index("unpark_head_XY()"),
                        recovery.index("unpark_head_ZE()"))

    def test_parked_and_resuming_states_hold_host_fifo(self):
        source = (ROOT / "src/common/marlin_server_types/marlin_server_state.h"
                  ).read_text()
        policy = source.split(
            "bool serial_print_fifo_held(bool serial_job, State state) {",
            1)[1].split("\n}", 1)[0]
        for required in ("serial_job", "State::Paused",
                         "State::Pausing_ParkHead", "is_resuming_state"):
            self.assertIn(required, policy)
        self.assertNotIn("State::Pausing_WaitIdle",
                         policy)  # Drain before parking.

    def test_injected_recovery_commands_run_and_held_fifo_is_not_busy(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        advance = source.split("void GCodeQueue::advance() {", 1)[1]
        self.assertLess(advance.index("process_injected_command()"),
                        advance.index("serial_print_fifo_held()"))
        self.assertLess(advance.index("serial_print_fifo_held()"),
                        advance.index("gcode.process_next_command()"))
        busy = source.split("bool GCodeQueue::has_commands_queued() {",
                            1)[1].split("\n}", 1)[0]
        self.assertIn("!marlin_server::serial_print_fifo_held()", busy)
        self.assertIn("|| injected_commands_P", busy)


if __name__ == "__main__":
    unittest.main()

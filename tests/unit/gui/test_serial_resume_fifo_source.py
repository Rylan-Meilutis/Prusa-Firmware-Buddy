"""Guard queued host moves while INDX pause recovery owns the tool."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class SerialResumeFifoTests(unittest.TestCase):

    def test_native_mmu_resume_keeps_xy_before_z_and_retract_accounting(self):
        source = (ROOT / "src/common/marlin_server.cpp").read_text()
        unpark = source.split("void unpark_head_ZE(void) {",
                              1)[1].split("\n}", 1)[0]
        self.assertIn("server.resume.pos.e - current_position.e", unpark)
        self.assertIn("plan_move_by(PAUSE_PARK_RETRACT_FEEDRATE", unpark)
        self.assertLess(
            unpark.index("destination.z = server.resume.pos.z"),
            unpark.index("server.resume.pos.e - current_position.e"))
        recovery = source.split("case State::Resuming_UnparkHead_XY:",
                                1)[1].split(
                                    "case State::Resuming_UnparkHead_ZE:",
                                    1)[0]
        self.assertLess(recovery.index("planner.processing()"),
                        recovery.index("unpark_head_ZE()"))

    def test_parked_and_resuming_states_hold_host_fifo(self):
        source = (ROOT / "src/common/marlin_server_types/marlin_server_state.h"
                  ).read_text()
        policy = source.split(
            "bool serial_print_fifo_held(bool serial_job, State state) {",
            1)[1].split("\n}", 1)[0]
        for required in ("serial_job", "State::Paused", "is_pausing_state",
                         "is_resuming_state"):
            self.assertIn(required, policy)
        # Held job moves are not active pause housekeeping; runtime tests
        # verify this does not deadlock Pausing_WaitIdle.

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

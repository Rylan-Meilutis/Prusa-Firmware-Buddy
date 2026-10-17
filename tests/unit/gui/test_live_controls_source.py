"""Regression coverage for non-motion controls during blocking waits."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class LiveControls(unittest.TestCase):

    def test_local_setpoints_have_independent_bounded_queue(self):
        client = (ROOT / "src/common/marlin_client.cpp").read_text()
        self.assertIn(
            "Request::Type::SetVariable ? live_request_queue : request_queue",
            client)
        server = (ROOT / "src/common/marlin_server.cpp").read_text()
        cycle = server.split("static void cycle() {")[1].split(
            "static bool pre_finalize_print")[0]
        self.assertLess(cycle.index("live_request_queue.try_receive"),
                        cycle.index("if (is_cycle_running)"))
        self.assertLess(cycle.index("server_update_vars();"),
                        cycle.index("if (is_cycle_running)"))
        self.assertGreater(
            cycle.index("request_queue.try_receive(request, 0)",
                        cycle.index("if (is_cycle_running)")),
            cycle.index("if (is_cycle_running)"))

    def test_remote_controls_do_not_reparse_or_inject_gcode(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        controls = source.split('command.starts_with("@RME CONTROL SET ")'
                                )[1].split("#if HAS_CHAMBER_API()")[0]
        self.assertIn("count == 1", controls)
        self.assertIn("thermalManager.setTargetHotend", controls)
        self.assertIn("thermalManager.setTargetBed", controls)
        self.assertNotIn("enqueue", controls)
        self.assertNotIn("parser.", controls)

    def test_selected_firmware_is_checked_before_reboot_handoff(self):
        source = (ROOT / "src/marlin_stubs/M997.cpp").read_text()
        self.assertLess(source.index("rme_firmware_compatibility::compatible"),
                        source.index("set_reflash_bbf_sfn"))

    def test_load_purge_and_unload_eject_indx_pellets(self):
        source = (ROOT / "src/marlin_stubs/pause/pause.cpp").read_text()
        purge = source.split("bool Pause::nozzle_cleaner_purge_sequence()"
                             )[1].split("#endif // HAS_NOZZLE_CLEANER()")[0]
        unload = source.split("void Pause::unload_nozzle_clean_process")[
            1].split("void Pause::unload_finish_or_change_process")[0]
        for operation in (purge, unload):
            self.assertIn("Sequence::eject_blob", operation)
            self.assertIn("#if HAS_INDX()", operation)

    def test_serial_rx_is_polled_when_normal_fifo_is_full(self):
        source = (ROOT / "lib/Marlin/Marlin/src/Marlin.cpp").read_text()
        self.assertIn("queue.get_available_commands();", source)
        self.assertNotIn(
            "if (queue.length < BUFSIZE) queue.get_available_commands()",
            source)
        queue = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        self.assertIn("length < recovery_capacity", queue)

    def test_chamber_ui_uses_mailbox_not_motion_injection(self):
        source = (
            ROOT /
            "src/gui/menu_item/specific/menu_items_chamber.cpp").read_text()
        self.assertIn("request_target_temperature", source)
        self.assertNotIn("marlin_client::inject", source)
        chamber = (ROOT / "src/feature/chamber/chamber.cpp").read_text()
        self.assertIn("pending_target_.exchange(-1)", chamber)
        self.assertIn("pending_target_.store(-1)", chamber)

    def test_chamber_wait_tracks_changes_and_off(self):
        source = (
            ROOT /
            "src/marlin_stubs/feature/chamber/M141_M191.cpp").read_text()
        self.assertIn("target = *live_target", source)
        self.assertIn("if (!live_target.has_value())", source)

    def test_serial_actions_match_local_print_states(self):
        source = (ROOT / "src/gui/screen_printing_serial.cpp").read_text()
        action = source.split(
            "void screen_printing_serial_data_t::update_action_buttons")[1]
        for state in ("Pausing_Begin", "Pausing_WaitIdle", "Pausing_ParkHead",
                      "Aborting_Begin", "Aborting_WaitIdle",
                      "Aborting_ParkHead", "Resuming_Reheating",
                      "Resuming_UnparkHead_ZE"):
            self.assertIn("State::" + state, action)
        for label in ('_("PAUSING ...")', '_("ABORTING ...")',
                      "LabelRes::Pausing", "LabelRes::Resuming",
                      "LabelRes::Reheating"):
            self.assertIn(label, action)


if __name__ == "__main__":
    unittest.main()

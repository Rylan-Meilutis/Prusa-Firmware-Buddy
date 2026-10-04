"""Regression for the ce5dfc82a INDX no-tool homing crash dump."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class IndxExtruderCoordinateReset(unittest.TestCase):

    def test_no_tool_reset_uses_shared_motor_and_updates_both_positions(self):
        source = (ROOT /
                  "lib/Marlin/Marlin/src/module/planner.cpp").read_text()
        reset = source.split("void Planner::set_e_position_mm(")[1].split(
            "void Planner::reset_position()")[0]
        indx = reset.split("#if HAS_INDX()")[1].split("#else")[0]
        self.assertIn("e_axis_index = E0_AXIS;", indx)
        self.assertNotIn("return;", indx)
        self.assertIn("if(!e_axis_index.has_value())", reset)
        self.assertIn("position.e = LROUND", reset)
        self.assertIn("position_float.e = e;", reset)
        self.assertIn("buffer_sync_block();", reset)
        self.assertIn("stepper.set_axis_position(E_AXIS", reset)

    def test_real_no_tool_extrusion_still_rejected(self):
        source = (ROOT /
                  "lib/Marlin/Marlin/src/module/planner.cpp").read_text()
        self.assertIn(
            'if (!hints.move.is_service_extruder_move && target.e != position.e)',
            source)
        self.assertIn(
            'if(!tools.physical_tool.has_value()) {\n      bsod("E move without tool");',
            source)

    def test_g92_and_internal_resets_sync_planner(self):
        motion = (ROOT / "lib/Marlin/Marlin/src/module/motion.cpp").read_text()
        reset = motion.split("void sync_e_position_to(float e)")[1].split(
            "\n}")[0]
        self.assertIn("sync_plan_position_e();", reset)
        g92 = (ROOT /
               "lib/Marlin/Marlin/src/gcode/geometry/G92.cpp").read_text()
        self.assertIn("else if (didE) sync_plan_position_e();", g92)


if __name__ == "__main__":
    unittest.main()

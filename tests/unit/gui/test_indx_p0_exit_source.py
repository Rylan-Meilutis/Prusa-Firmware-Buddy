"""Guard P0's native INDX escape without relaxing external move validation."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class IndxParkExit(unittest.TestCase):

    def test_exit_only_after_success_and_without_z_move(self):
        source = (ROOT / "src/marlin_stubs/P.cpp").read_text()
        self.assertIn(
            "if (!tool_change(NoTool {}, return_type, z_lift, z_down))",
            source)
        exit_ = source.split("// The last dock")[1]
        self.assertIn("mapi::park", exit_)
        self.assertIn(
            "std::min(current_position.x, float(X_WASTEBIN_SAFE_POINT))",
            exit_)
        self.assertIn(
            "std::max(current_position.y, float(Y_DOCK_PARKING_MIN_SAFE_POS))",
            exit_)
        self.assertNotIn(".z =", exit_)

    def test_native_route_leaves_docks_before_crossing_x(self):
        source = (ROOT / "src/common/mapi/parking.cpp").read_text()
        move = source.split("const auto move_xy =")[1].split(
            "const bool move_xy_first")[0]
        self.assertLess(move.index("p.move_y(Y_DOCK_PARKING_MIN_SAFE_POS)"),
                        move.index("p.pre_park_move_pattern"))


if __name__ == "__main__":
    unittest.main()

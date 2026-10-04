"""Keep successful calibration durable independently of print completion."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class CalibrationPersistence(unittest.TestCase):

    def test_success_commits_before_cleanup_and_completion(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        success = source.split(
            "const buddy::extrusion_calibration::Result result { best_pa")[1]
        self.assertLess(success.index("save_cache("),
                        success.index("cleanup(slot"))
        self.assertLess(success.index("save_cache("),
                        success.index("emit_pressure_advance_gcode("))
        self.assertNotIn("pa_cache::invalidate", source)

    def test_job_reset_only_clears_ram(self):
        source = (ROOT / "src/feature/extrusion_calibration.cpp").read_text()
        reset = source.split("void reset_job_results()")[1].split("\n}")[0]
        self.assertNotIn("invalidate", reset)
        self.assertNotIn("unlink", reset)

    def test_fan_owned_once_by_batch_after_cache_check(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        self.assertEqual(source.count("DockFanGuard dock_cooling"), 1)
        batch = source.split("bool run_batch(")[1]
        self.assertLess(batch.index("restore_cache("),
                        batch.index("DockFanGuard dock_cooling"))
        self.assertIn("if (!cached[i])", batch)
        guard = source.split("class DockFanGuard")[1].split("#endif")[0]
        self.assertIn("enabled(uncached_tools > 1)", guard)
        self.assertIn("set_pwm(previous_pwm)", guard)


if __name__ == "__main__":
    unittest.main()

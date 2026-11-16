"""Guard the opt-in slicer optimization against the supplied original profile."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
TEMPLATE = ROOT / "doc/gcode/templates/orca_indx_fast_toolchange.gcode"
ORIGINAL = ROOT / "tests/unit/gui/fixtures/orca_indx_original_toolchange.gcode"


def branches(text):
    return text.split("{else}\n  {if previous_extruder != -1}", 1)


def commands(text, expression):
    return [
        line.split(";", 1)[0].strip() for line in text.splitlines()
        if re.match(expression, line.strip())
    ]


class OrcaIndxFastToolchangeTests(unittest.TestCase):

    def test_both_branches_have_entry_eject_and_explicit_optional_exit(self):
        text = TEMPLATE.read_text()
        self.assertIn("{local rme_eject_on_exit = false}", text)
        for branch in branches(text):
            self.assertEqual(len(commands(branch, r"G12 S30\b")), 2)
            self.assertLess(branch.index("G12 S90"),
                            branch.index("G12 S30 F1"))
            self.assertLess(branch.index("G12 S30 F1"),
                            branch.index("M109 S{target_temp}"))
            optional = branch.split("{if rme_eject_on_exit}",
                                    1)[1].split("{endif}", 1)[0]
            self.assertEqual(commands(optional, r"G12 S30\b"), ["G12 S30 F1"])
            self.assertLess(branch.index("{if rme_eject_on_exit}"),
                            branch.index("G12 S91"))
            self.assertEqual(len(commands(branch, r"G12 S1\b")), 2)
        self.assertFalse(commands(text, r"G4\b"))
        self.assertFalse(commands(text, r"P0\b"))
        self.assertFalse(commands(text, r"G750\b"))

    def test_purge_retraction_temperature_and_motion_limits_are_preserved(
            self):
        old = branches(ORIGINAL.read_text())
        new = branches(TEMPLATE.read_text())
        self.assertEqual(len(old), 2)
        self.assertEqual(len(new), 2)
        for before, after in zip(old, new):
            for pattern in (r"G1 E", r"G1 Y", r"G27\b", r"T\{", r"M201\b",
                            r"M109\b", r"G0 Z", r"M204\b"):
                self.assertEqual(commands(before, pattern),
                                 commands(after, pattern))
            self.assertIn(
                "{e_retracted[next_extruder] = 1.5 * retraction_length[next_extruder]}",
                after)
            self.assertIn("M83", after)
        self.assertIn("min(travel_speed, 350.0) * 60", TEMPLATE.read_text())
        no_tower, tower = new
        self.assertEqual(len(commands(no_tower, r"M400\b")), 2)
        self.assertEqual(len(commands(tower, r"M400\b")), 0)
        self.assertRegex(
            no_tower,
            r"M400[^\n]*\n\s*M201 E\[machine_max_acceleration_e\]\n\s*M906 P0")
        self.assertFalse(
            re.search(r"G12 S1[^\n]*\n\s*M400", TEMPLATE.read_text()))

    def test_removed_barriers_and_park_are_redundant_in_production_firmware(
            self):
        cleaner = (
            ROOT /
            "src/feature/nozzle_cleaner/nozzle_cleaner.cpp").read_text()
        function = cleaner.split("bool load_and_execute(Sequence seq)",
                                 1)[1].split("bool is_loader_idle()", 1)[0]
        self.assertRegex(
            function,
            r"#if HAS_INDX\(\)[\s\S]*planner.synchronize\(\);\n#endif\n\s*return true;"
        )
        changer = (ROOT /
                   "lib/Marlin/Marlin/src/module/prusa/toolchanger_indx.cpp"
                   ).read_text()
        change = changer.split("bool PrusaToolChanger::tool_change(",
                               1)[1].split("/// Lower the bed", 1)[0]
        self.assertIn("if (!park(std::get<PhysicalToolIndex>(old_tool)))",
                      change)
        self.assertIn("if (!pickup(std::get<PhysicalToolIndex>(new_tool)))",
                      change)
        self.assertLess(change.index("if (!park("),
                        change.index("if (!pickup("))
        motor = (ROOT / "lib/Marlin/Marlin/src/gcode/feature/trinamic/M906.cpp"
                 ).read_text()
        profile_handler = motor.split("if (parser.seen('P'))",
                                      1)[1].split("bool report", 1)[0]
        self.assertIn("set_active_motor_current_profile", profile_handler)
        self.assertNotIn("planner.synchronize", profile_handler)

    def test_documented_nominal_extrusion_times_are_not_whole_change_promises(
            self):
        times = [
            8 / 1200 * 60, 3.4 / 131.028 * 60, 2.49578 / 174.704 * 60,
            1.5 / 174.704 * 60, 1.5 / 250 * 60, 1.05 / 2700 * 60
        ]
        self.assertAlmostEqual(sum(times), 3.713, places=3)
        doc = (ROOT / "doc/gcode/indx_fast_toolchange.md").read_text()
        self.assertIn("3.713 s", doc)
        self.assertIn("no `G4` timed dwell", doc)
        self.assertIn("150 ms", doc)
        self.assertIn("1 s residency", doc)


if __name__ == "__main__":
    unittest.main()

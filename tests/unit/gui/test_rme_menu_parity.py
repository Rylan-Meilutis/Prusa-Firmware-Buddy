"""Keep the RME 6.9.2 menu entry points reachable after upstream ports."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]


class RmeMenuParity(unittest.TestCase):

    def test_control_filtration_shortcuts_remain_hardware_guarded(self):
        source = (ROOT / "src/gui/screen_menu_control.hpp").read_text()
        self.assertRegex(
            source,
            r"#if HAS_CHAMBER_FILTRATION_API\(\)\s+MI_CHAMBER_FILTER_CYCLE,\s+MI_CHAMBER_FILTRATION,\s+#endif"
        )

    def test_settings_keep_rme_entry_points(self):
        source = (ROOT / "src/gui/screen_menu_settings.hpp").read_text()
        menu = source.split("using ScreenMenuSettingsBase", 1)[1]
        for item in ("MI_SERIAL_PRINTING_SETTINGS", "MI_LOCK_SETTINGS",
                     "MI_HEATER_SAFETY_SETTINGS", "MI_PID_SETTINGS",
                     "MI_EXPORT_RME_SETTINGS", "MI_CUSTOM_FILAMENT_COLORS",
                     "MI_CUSTOM_FILAMENT_MANUFACTURERS",
                     "MI_TOOLHEAD_SETTINGS", "MI_LIGHTS", "MI_WASTEBIN",
                     "MI_CHAMBER_FILTRATION"):
            with self.subTest(item=item):
                self.assertRegex(menu, r"\b" + re.escape(item) + r"\b")
        # Custom colors/manufacturers keep their pre-existing MINI exclusion.
        self.assertRegex(
            menu,
            r"#if !HAS_MINI_DISPLAY\(\)\s+MI_CUSTOM_FILAMENT_COLORS,\s+MI_CUSTOM_FILAMENT_MANUFACTURERS,"
        )


if __name__ == "__main__":
    unittest.main()

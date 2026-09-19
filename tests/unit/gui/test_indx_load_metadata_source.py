"""Source-contract checks for deferred INDX metadata screen transitions.

Hardware validation is still needed; these guard the Close/Open ordering
required by Screens::InnerLoop without pretending to emulate the GUI.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / "src/gui/screen/screen_preheat.cpp").read_text()


def class_source(name):
    start = SOURCE.index("class " + name + " final")
    return SOURCE[start:SOURCE.index("\n};", start)]


class IndxLoadMetadataTransitions(unittest.TestCase):

    def test_color_is_replaced_before_opening_manufacturer(self):
        source = class_source("MI_LOAD_COLOR")
        save = source.index("filament::set_color_to_load(color_)")
        close = source.index("Screens::Access()->Close();")
        open_brand = source.index(
            "Screens::Access()->Open<ScreenLoadManufacturer>();")
        self.assertLess(save, close)
        self.assertLess(close, open_brand)
        self.assertEqual(source.count("Screens::Access()->Close();"), 1)

    def test_existing_and_new_manufacturer_finish_once(self):
        for name in ("MI_LOAD_MANUFACTURER", "MI_LOAD_NEW_MANUFACTURER"):
            with self.subTest(handler=name):
                source = class_source(name)
                self.assertEqual(source.count("FSM_response_variant("), 1)
                self.assertEqual(source.count("Screens::Access()->Close();"),
                                 1)
                self.assertNotIn("Open<ScreenLoadColor>", source)
                self.assertLess(source.index("set_manufacturer_to_load("),
                                source.index("FSM_response_variant("))


if __name__ == "__main__":
    unittest.main()

"""Regression checks for serial-print page routing and retained results."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / "src/gui/screen_printing_serial.cpp").read_text()
HEADER = (ROOT / "src/gui/screen_printing_serial.hpp").read_text()


class SerialPrintPages(unittest.TestCase):

    def test_no_duplicate_messages_page_or_history_collector(self):
        for token in ("Page::message", "update_messages",
                      "message_page_available", "walk_history",
                      "append_message_line", "Messages %"):
            self.assertNotIn(token, SOURCE)
        self.assertNotIn("last_message_id", HEADER)

    def test_results_and_filtration_remain(self):
        self.assertIn("set_page(Page::result)", SOURCE)
        for token in ("PRINT FINISHED", "PRINT CANCELED", "Filtering left",
                      "update_finished_summary", "advance_finished_stat"):
            self.assertIn(token, SOURCE)

    def test_navigation_only_cycles_print_time_items(self):
        advance = SOURCE.split(
            "void screen_printing_serial_data_t::advance_page()")[1].split(
                "void screen_printing_serial_data_t::retreat_page()")[0]
        self.assertIn("next_time_item(current_time_item)", advance)
        self.assertIn("set_page(Page::progress)", advance)
        self.assertNotIn("Page::result", advance)

    def test_no_live_message_buffer(self):
        self.assertIn("std::array<char, 64> result_text", HEADER)
        self.assertNotIn("message_text", HEADER)


if __name__ == "__main__":
    unittest.main()

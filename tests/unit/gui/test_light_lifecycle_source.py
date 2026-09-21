"""Guard lighting integration paths which depend on board feature selection."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / "src/leds/side_strip_handler.cpp").read_text()


class LightLifecycle(unittest.TestCase):

    def test_external_output_reported_on_standard_extension_too(self):
        report = SOURCE.split(
            "uint8_t SideStripHandler::current_chamber_brightness() const {"
        )[1].split("\n}")[0]
        self.assertIn("external_light_bar::is_on()", report)
        self.assertNotIn("!XBUDDY_EXTENSION_VARIANT_IS_STANDARD()", report)

    def test_idle_modes_do_not_leak_through_print_session(self):
        entry = SOURCE.split(
            "if (print_active && !print_override_session_active) {")[1].split(
                "} else")[0]
        self.assertIn("chamber_mode_state.mode = -1", entry)
        exit_ = SOURCE.split(
            "else if (!print_active && terminal_print_state && print_override_session_active) {"
        )[1].split("\n        if (print_active)")[0]
        self.assertIn(
            "!print_active && terminal_print_state && print_override_session_active",
            SOURCE)
        self.assertIn("restart_idle_countdown(time_ms)", exit_)
        self.assertIn("screen_forced_off = false", exit_)

    def test_print_lcd_on_is_not_a_temporary_wake(self):
        setter = SOURCE.split(
            "void SideStripHandler::set_screen_on(const bool on) {")[1].split(
                "void SideStripHandler::restart_idle_countdown")[0]
        self.assertIn("if (print_active_for_leds())", setter)
        self.assertIn("print_screen_brightness_override = 0", setter)
        self.assertIn("print_screen_brightness_overridden = !on", setter)


if __name__ == "__main__":
    unittest.main()

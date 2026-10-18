"""Execute the production LCD setter with both panel drivers stubbed."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class LcdBrightnessUpdates(unittest.TestCase):

    @unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
    def test_unchanged_brightness_does_not_touch_panel(self):
        source = (ROOT / "src/leds/led_manager.cpp").read_text()
        setter = source.split("void LEDManager::set_lcd_brightness", 1)[1]
        setter = "void LEDManager::set_lcd_brightness" + setter.rsplit(
            "} // namespace leds", 1)[0]
        prelude = r"""
#include <cstdint>
#include <cassert>
int panel_writes = 0, clears = 0, strip_updates = 0;
struct LEDManager { bool lcd_brightness_off; void set_lcd_brightness(uint8_t); };
struct ColorRGBW { unsigned data; ColorRGBW(int,int,int): data(0) {} };
struct Strip {
 void set_all(int) {} void force_refresh(int) {} void set(unsigned,int) {}
 void update() { ++strip_updates; }
} strip;
Strip &get_status_leds() { return strip; }
void osDelay(int) {}
"""
        for panel in ("st7789v", "ili9488"):
            with self.subTest(
                    panel=panel), tempfile.TemporaryDirectory() as directory:
                stubs = f"""
#define HAS_ST7789_DISPLAY() {int(panel == 'st7789v')}
#define HAS_ILI9488_DISPLAY() {int(panel == 'ili9488')}
void {panel}_suppress_display_writes(bool) {{ ++panel_writes; }}
void {panel}_brightness_set(int) {{ ++panel_writes; }}
void {panel}_brightness_enable() {{ ++panel_writes; }}
void {panel}_brightness_disable() {{ ++panel_writes; }}
void {panel}_cmd_dispoff() {{ ++panel_writes; }}
void {panel}_cmd_slpin() {{ ++panel_writes; }}
void {panel}_cmd_slpout() {{ ++panel_writes; }}
void {panel}_cmd_dispon() {{ ++panel_writes; }}
void st7789v_delay_ms(int) {{}}
void st7789v_clear(int) {{ ++clears; }}
void ili9488_clear_full_black() {{ ++clears; }}
"""
                main = r"""
int main() {
 LEDManager manager;
 for (int brightness : {100, 50, 0, 0, 75, 100}) {
  manager.set_lcd_brightness(brightness);
  int writes = panel_writes, black_clears = clears;
  for (int i=0; i<100; ++i) manager.set_lcd_brightness(brightness);
  assert(writes == panel_writes && black_clears == clears);
  assert(manager.lcd_brightness_off == (brightness == 0));
 }
 assert(clears == 1);
#if HAS_ILI9488_DISPLAY()
 assert(strip_updates == 606);
#endif
}
"""
                cpp = Path(directory) / "test.cpp"
                cpp.write_text("#include <initializer_list>\n" + prelude +
                               stubs + setter + main)
                binary = Path(directory) / "test"
                subprocess.run(
                    ["c++", "-std=c++17",
                     str(cpp), "-o",
                     str(binary)],
                    check=True)
                subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()

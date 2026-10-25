"""Execute the production LCD setter with both panel drivers stubbed."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class LcdBrightnessUpdates(unittest.TestCase):

    @unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
    def test_clock_and_serial_result_refresh_only_changed_widgets(self):
        header = (ROOT / "src/gui/window_header.cpp").read_text()
        clock = "void window_header_t::updateTime()" + header.split(
            "void window_header_t::updateTime()", 1)[1].split(
                "void window_header_t::update_bed_info()", 1)[0]
        source = (ROOT / "src/gui/screen_printing_serial.cpp").read_text()
        actions = "void screen_printing_serial_data_t::update_action_buttons" + source.split(
            "void screen_printing_serial_data_t::update_action_buttons",
            1)[1].split(
                "void screen_printing_serial_data_t::update_progress()", 1)[0]
        loop = source.split("case GUI_event_t::LOOP:",
                            1)[1].split("case GUI_event_t::HELD_RELEASED:",
                                        1)[0]
        self.assertEqual(loop.count("update_action_buttons(state)"), 1)
        self.assertIn("else if (lock_buttons_applied)", loop)
        code = r"""
#include <cassert>
#include <array>
#include <string>
#define HAS_MINI_DISPLAY() 0
namespace time_tools {
 bool changed=false;
 bool update_time() { bool v=changed; changed=false; return v; }
}
struct Widget { int invalidations=0; void Invalidate() { ++invalidations; } };
struct window_header_t {
 Widget time_val; int full_clears=0;
 void Invalidate() { ++full_clears; }
 void updateTime();
};
namespace marlin_server { enum class State {
 Finished, Aborted, Paused, Printing, Idle, SerialPrintInit, Pausing_Begin, Pausing_WaitIdle,
 Pausing_ParkHead, Resuming_Begin, Resuming_BufferData, Resuming_ExecutingGCodeInterrupt,
 Resuming_UnparkHead_XY, Resuming_UnparkHead_ZE, Resuming_Reheating,
 Aborting_Begin, Aborting_WaitIdle, Aborting_UnloadFilament, Aborting_ParkHead, Aborting_Preview
}; }
bool is_print_result(marlin_server::State state) {
 return state==marlin_server::State::Finished || state==marlin_server::State::Aborted;
}
enum class BtnSocket { Left, Middle, Right };
enum class BtnRes {Settings, Disconnect, SetReady, Reprint, Resume, Pause};
enum class LabelRes {Settings, Stop, Continue, Reprint, Resume, Pause, Pausing, Resuming, Reheating};
#define _(text) text
struct screen_printing_serial_data_t {
 static constexpr const char *caption="PRINTING";
 struct Header {
  std::string title; int changes=0;
  void SetText(const char *value) { if (title!=value) ++changes; title=value; }
 } header;
 std::array<int,3> icons{-1,-1,-1}, labels{-1,-1,-1}; int changes=0;
 void SetButtonIconAndLabel(BtnSocket s, BtnRes i, LabelRes l) {
  auto n=int(s);
  if (icons[n]!=int(i) || labels[n]!=int(l)) ++changes;
  icons[n]=int(i); labels[n]=int(l);
 }
 void EnableButton(BtnSocket) {} void DisableButton(BtnSocket) {}
 void update_action_buttons(marlin_server::State);
};
"""
        main = r"""
int main() {
 window_header_t header;
 time_tools::changed=true;
 for (int i=0; i<1000; ++i) header.updateTime();
 assert(header.full_clears==0 && header.time_val.invalidations==1);
 for (auto state : {marlin_server::State::Finished, marlin_server::State::Aborted}) {
  screen_printing_serial_data_t screen;
  screen.update_action_buttons(state);
  int changes=screen.changes;
  int title_changes=screen.header.changes;
  for (int i=0; i<1000; ++i) screen.update_action_buttons(state);
  assert(screen.changes==changes);
  assert(screen.header.changes==title_changes);
  assert(screen.labels[2]==int(LabelRes::Continue));
  screen.update_action_buttons(marlin_server::State::Printing);
  assert(screen.labels[2]==int(LabelRes::Stop));
 }
}
"""
        with tempfile.TemporaryDirectory() as directory:
            cpp, binary = Path(directory) / "test.cpp", Path(
                directory) / "test"
            cpp.write_text(code + clock + actions + main)
            subprocess.run(["c++", "-std=c++17",
                            str(cpp), "-o",
                            str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True)

    @unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
    def test_unchanged_finished_header_does_not_invalidate(self):
        text = (ROOT / "src/gui/window_header.cpp").read_text()
        setter = "void window_header_t::SetText" + text.split(
            "void window_header_t::SetText", 1)[1].split(
                "void window_header_t::set_show_bed_info", 1)[0]
        code = r"""
#include <cassert>
struct string_view_utf8 {
 const char *ref;
 bool is_same_ref(const string_view_utf8 &other) const { return ref == other.ref; }
};
struct Label {
 string_view_utf8 value { nullptr };
 int invalidations = 0;
 auto &GetText() { return value; }
 void SetText(const string_view_utf8 &v) { value = v; ++invalidations; }
};
struct window_header_t {
 Label label; int invalidations = 0;
 void Invalidate() { ++invalidations; }
 void SetText(const string_view_utf8 &);
};
"""
        main = r"""
int main() {
 window_header_t header;
 const string_view_utf8 finished { "PRINT FINISHED" }, canceled { "PRINT CANCELED" };
 for (int i=0; i<1000; ++i) header.SetText(finished);
 assert(header.invalidations == 0 && header.label.invalidations == 1);
 for (int i=0; i<1000; ++i) header.SetText(canceled);
 assert(header.invalidations == 0 && header.label.invalidations == 2);
}
"""
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            cpp.write_text(code + setter + main)
            binary = Path(directory) / "test"
            subprocess.run(["c++", "-std=c++17",
                            str(cpp), "-o",
                            str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True)

    @unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
    def test_unchanged_brightness_does_not_touch_panel(self):
        source = (ROOT / "src/leds/led_manager.cpp").read_text()
        setter = source.split("void LEDManager::set_lcd_brightness", 1)[1]
        setter = "void LEDManager::set_lcd_brightness" + setter.rsplit(
            "} // namespace leds", 1)[0]
        prelude = r"""
#include <cstdint>
#include <cassert>
#include <array>
int panel_writes = 0, clears = 0, strip_updates = 0;
struct LEDManager { bool lcd_brightness_off; void set_lcd_brightness(uint8_t); };
struct ColorRGBW { unsigned data; ColorRGBW(int,int g,int): data(g) {} };
struct Strip {
 std::array<unsigned,4> pixels{0x123456,0x234567,0x345678,0};
 void set_all(int value) { pixels.fill(value); } void force_refresh(int) {}
 void set(unsigned color,int index) { pixels[index]=color; }
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
#if HAS_ILI9488_DISPLAY()
  // Sleeping, waking, and changing the LCD must not touch status pixels.
  assert(strip.pixels[0]==0x123456 && strip.pixels[1]==0x234567
         && strip.pixels[2]==0x345678);
  assert(strip.pixels[3]==unsigned(brightness*255/100));
#endif
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

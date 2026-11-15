"""Exercise shared production browser navigation without GUI hardware."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class SharedFileBrowserTests(unittest.TestCase):

    def test_selection_policies_remain_separate(self):
        expected = (("screen_filebrowser", "printTheFile", "PRINTABLE"),
                    ("screen_firmware_update", "select_file",
                     "FIRMWARE"), ("screen_theme_filebrowser",
                                   "load_selected_file", "PRINTABLE"))
        for filename, action, filter_name in expected:
            with self.subTest(browser=filename):
                header = (ROOT / "src/gui" / (filename + ".hpp")).read_text()
                source = (ROOT / "src/gui" / (filename + ".cpp")).read_text()
                self.assertIn("public FileBrowserScreen", header)
                self.assertIn("static void " + action + "(const char *path)",
                              header)
                self.assertIn("::" + action + ",", source)
                self.assertIn("FileSort::FileFilter::" + filter_name, source)
        firmware = (ROOT / "src/gui/screen_firmware_update.cpp").read_text()
        self.assertIn("stage_firmware_for_bootloader(path)", firmware)
        self.assertIn('gcode("M997 /usb/FWUPD.RME")', firmware)
        self.assertIn("Responses_YesNo", firmware)
        theme = (ROOT / "src/gui/screen_theme_filebrowser.cpp").read_text()
        self.assertIn("ScreenThemeFileBrowser::on_media_removed()", theme)
        self.assertIn("Screens::Access()->Get()->Validate()", theme)

    def test_shared_events_dispatch_selection_and_media_callbacks(self):
        source = (ROOT / "src/gui/screen_filebrowser.cpp").read_text()

        def implementation(name):
            start = source.index("void FileBrowserScreen::" + name)
            end = source.find("\nvoid ", start + 1)
            return source[start:] if end < 0 else source[start:end]

        production = "\n".join(
            implementation(name)
            for name in ("select_file", "windowEvent", "checkMissingMedia",
                         "goHome", "close_browser"))
        # Production pointers and int are both 32-bit on the MCU; the host is 64-bit.
        production = production.replace("reinterpret_cast<int>(param)",
                                        "reinterpret_cast<intptr_t>(param)")
        program = r'''
#include <cassert>
#include <cstdint>
#include <array>
#include <cstring>
namespace filename_defs { constexpr unsigned path_buffer_size=32; }
struct window_t {};
enum class GUI_event_t { MEDIA, LOOP, CHILD_CLICK, OTHER };
enum class MediaState_t : intptr_t { inserted, removed, error };
struct GuiMediaEventsHandler {
 static MediaState_t state;
 static MediaState_t Get() { return state; }
};
MediaState_t GuiMediaEventsHandler::state=MediaState_t::inserted;
class FileBrowserScreen;
static FileBrowserScreen *active;
struct Screens {
 static Screens *Access() { static Screens s; return &s; }
 void Close();
};
struct WindowFileBrowser {
 int cleared=0, saved=0, written=12;
 void SaveTopSFN() { ++saved; }
 void save_top_canonical() { ++saved; }
 int WriteNameToPrint(char *out, unsigned) { std::strcpy(out,"/usb/a.bbf"); return written; }
 void clear_first_visible_sfn() { ++cleared; }
 void clear_first_visible_canonical() { ++cleared; }
 union event_conversion_union {
  enum class Action { go_home, file_selected };
  void *pvoid;
  Action action;
 };
};
void selection_callback(const char *);
void media_callback();
void close_callback();
struct Policy {
 void (*on_file_selected)(const char *);
 void (*on_media_removed)();
 void (*close)();
 bool save_top=true;
};
class FileBrowserScreen {
 WindowFileBrowser files;
 void goHome();
 void checkMissingMedia(MediaState_t);
public:
 int selected=0, media=0, closed=0, close_callbacks=0, invalid=0;
 Policy configuration_ { selection_callback, nullptr, nullptr };
 virtual ~FileBrowserScreen()=default;
 WindowFileBrowser &browser() { return files; }
 virtual void on_file_selected()=0;
 virtual void on_media_removed() {}
 void close_screen() { ++closed; }
 void close_browser();
 void select_file();
 void windowEvent(window_t *,GUI_event_t,void *);
};
void Screens::Close() { active->close_screen(); }
void selection_callback(const char *path) {
 if(!path) { ++active->invalid; return; }
 assert(std::strcmp(path,"/usb/a.bbf")==0); active->on_file_selected();
}
void media_callback() { active->on_media_removed(); }
void close_callback() { active->close_screen(); ++active->close_callbacks; }
struct Projects : FileBrowserScreen { void on_file_selected() override { selected+=1; } };
struct Firmware : FileBrowserScreen {
 Firmware() { configuration_.close=close_callback; configuration_.save_top=false; }
 void on_file_selected() override { selected+=10; }
};
struct Themes : FileBrowserScreen {
 Themes() { configuration_.close=close_callback; configuration_.on_media_removed=media_callback; }
 void on_file_selected() override { selected+=100; }
 void on_media_removed() override { ++media; }
};
''' + production + r'''
int main() {
 Projects projects; Firmware firmware; Themes themes;
 for(FileBrowserScreen *screen : {static_cast<FileBrowserScreen *>(&projects), &firmware, &themes}) {
  active=screen;
  screen->windowEvent(nullptr, GUI_event_t::OTHER, nullptr);
  screen->windowEvent(nullptr, GUI_event_t::LOOP, nullptr);
  assert(screen->closed==0);
  screen->windowEvent(nullptr, GUI_event_t::CHILD_CLICK, reinterpret_cast<void *>(1));
  assert(screen->closed==0 && screen->selected>0);
  assert(screen->browser().saved==(screen==&firmware ? 0 : 1));
  for(int written : {-1,32,33}) {
   screen->browser().written=written;
   screen->select_file();
  }
  assert(screen->invalid==3);
  screen->windowEvent(nullptr, GUI_event_t::CHILD_CLICK, nullptr);
  assert(screen->closed==1 && screen->browser().cleared==1);
  GuiMediaEventsHandler::state=MediaState_t::removed;
  screen->windowEvent(nullptr, GUI_event_t::LOOP, nullptr);
  assert(screen->closed==2 && screen->browser().cleared==2);
  screen->windowEvent(nullptr, GUI_event_t::MEDIA, reinterpret_cast<void *>(1));
  assert(screen->closed==3 && screen->browser().cleared==3);
  GuiMediaEventsHandler::state=MediaState_t::inserted;
 }
 assert(projects.selected==1 && firmware.selected==10 && themes.selected==100);
 assert(projects.media==0 && firmware.media==0 && themes.media==2);
 assert(projects.close_callbacks==0 && firmware.close_callbacks==3 && themes.close_callbacks==3);
}
'''
        program = program.replace(
            "#include <cassert>",
            "#include <cassert>\n#include <initializer_list>")
        program = program.replace(
            "&firmware, &themes",
            "static_cast<FileBrowserScreen *>(&firmware), static_cast<FileBrowserScreen *>(&themes)"
        )
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "browser"
            subprocess.run([
                "c++", "-std=c++20", "-x", "c++",
                "-fsanitize=address,undefined", "-fno-pie", "-no-pie", "-o",
                str(executable), "-"
            ],
                           input=program,
                           text=True,
                           check=True)
            subprocess.run([str(executable)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

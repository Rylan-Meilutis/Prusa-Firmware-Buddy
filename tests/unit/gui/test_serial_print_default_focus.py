"""Exercise production serial button updates without changing media printing."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class SerialPrintDefaultFocusTests(unittest.TestCase):

    def test_production_button_updates_choose_tune_once_after_preparation(
            self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "src/gui/screen_printing_serial.cpp").read_text()
        begin = source.index(
            "void screen_printing_serial_data_t::update_action_buttons(")
        function = source[begin:source.index(
            "\nvoid screen_printing_serial_data_t::update_progress()", begin)]
        code = r'''
#include <cassert>
#include <cstddef>
#include <utility>
namespace marlin_server { enum class State { Idle, SerialPrintInit, Printing,
    Paused, Pausing, Resuming, Finished, Aborted }; }
bool is_print_result(marlin_server::State s) {
    return s==marlin_server::State::Finished || s==marlin_server::State::Aborted;
}
int focus=0, focus_changes=0;
struct Button {
    int id;
    bool enabled=true;
    void SetFocus() { focus=id; ++focus_changes; }
};
class screen_printing_serial_data_t {
public:
    enum class BtnSocket { Left=0, Middle=1, Right=2 };
    enum class BtnRes { Settings, SetReady, Disconnect, Reprint, Resume, Pause };
    enum class LabelRes { Settings, Continue, Stop, Reprint, Resume, Pause };
    Button buttons[3]={{0},{1},{2}};
    BtnRes icons[3]={BtnRes::Settings,BtnRes::Pause,BtnRes::Disconnect};
    LabelRes labels[3]={LabelRes::Settings,LabelRes::Pause,LabelRes::Stop};
    bool initial_tune_focus_pending=true;
    void SetButtonIconAndLabel(BtnSocket i,BtnRes icon,LabelRes label) {
        icons[std::to_underlying(i)]=icon; labels[std::to_underlying(i)]=label;
    }
    void EnableButton(BtnSocket i) { buttons[std::to_underlying(i)].enabled=true; }
    void DisableButton(BtnSocket i) {
        int n=std::to_underlying(i);
        if (!buttons[n].enabled) return;
        // Shared ScreenPrintingModel's exact rightward fallback rule.
        if (focus==n) for(int offset=1;offset<3;++offset) {
            if (buttons[(n+offset)%3].enabled) { buttons[(n+offset)%3].SetFocus(); break; }
        }
        buttons[n].enabled=false;
    }
    void update_action_buttons(marlin_server::State);
};
''' + function + r'''
using State=marlin_server::State;
int main() {
    screen_printing_serial_data_t screen;
    screen.update_action_buttons(State::SerialPrintInit);
    assert(focus==2 && !screen.buttons[0].enabled && !screen.buttons[1].enabled);
    screen.update_action_buttons(State::Printing);
    assert(focus==0 && screen.buttons[0].enabled && screen.buttons[1].enabled);
    assert(screen.icons[1]==screen_printing_serial_data_t::BtnRes::Pause);
    focus=2; int before=focus_changes; // User deliberately selects Cancel.
    screen.update_action_buttons(State::Printing);
    assert(focus==2 && focus_changes==before);
    screen.update_action_buttons(State::Pausing);
    screen.update_action_buttons(State::Paused);
    assert(focus==2 && screen.icons[1]==screen_printing_serial_data_t::BtnRes::Resume);
    focus=1; // Keep Resume usable without jumping to Tune on pause/resume updates.
    screen.update_action_buttons(State::Printing);
    assert(focus==1);
    screen.update_action_buttons(State::Finished);
    assert(focus==1 && screen.icons[1]==screen_printing_serial_data_t::BtnRes::Reprint);
    assert(screen.labels[2]==screen_printing_serial_data_t::LabelRes::Continue);
    focus=2; screen.update_action_buttons(State::Finished);
    assert(focus==2);
    screen.update_action_buttons(State::Printing); // New job with reused screen.
    assert(focus==0);
    screen.update_action_buttons(State::Aborted);
    assert(screen.labels[2]==screen_printing_serial_data_t::LabelRes::Continue);
    screen.update_action_buttons(State::SerialPrintInit);
    assert(focus==2);
    screen.update_action_buttons(State::Paused); // Host can start paused.
    assert(focus==0 && screen.icons[1]==screen_printing_serial_data_t::BtnRes::Resume);
    focus=1; screen.update_action_buttons(State::Paused); // Including unlock refresh.
    assert(focus==1);
    screen_printing_serial_data_t direct;
    focus=2; direct.update_action_buttons(State::Printing); // No preparation screen.
    assert(focus==0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "focus.cpp"
            executable = Path(directory) / "focus"
            cpp.write_text(code)
            result = subprocess.run([
                compiler, "-std=c++23", "-Wall", "-Wextra", "-Werror",
                str(cpp), "-o",
                str(executable)
            ],
                                    capture_output=True,
                                    text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(executable)],
                                    capture_output=True,
                                    text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_focus_is_serial_only_shared_across_layouts_and_respects_locked_routing(
            self):
        source = (ROOT / "src/gui/screen_printing_serial.cpp").read_text()
        header = (ROOT / "src/gui/screen_printing_serial.hpp").read_text()
        self.assertIn("bool initial_tune_focus_pending = true;", header)
        self.assertNotIn("initial_tune_focus_pending",
                         (ROOT / "src/gui/screen_printing.cpp").read_text())
        function = source.split(
            "void screen_printing_serial_data_t::update_action_buttons(",
            1)[1].split(
                "void screen_printing_serial_data_t::update_progress()", 1)[0]
        self.assertNotIn("HAS_MINI_DISPLAY", function)
        self.assertIn(
            "buttons[std::to_underlying(BtnSocket::Left)].SetFocus();",
            function)
        routing = source.split(
            "void screen_printing_serial_data_t::windowEvent(",
            1)[1].split("switch (event)", 1)[0]
        self.assertIn("if (printer_lock::locked())", routing)
        self.assertIn("show_locked_buttons();", routing)
        self.assertIn("} else {\n            update_action_buttons(state);",
                      routing)


if __name__ == "__main__":
    unittest.main()

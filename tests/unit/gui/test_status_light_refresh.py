"""UI invalidation must not publish one-frame deep idle to the status LEDs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class StatusLightRefresh(unittest.TestCase):

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_invalidated_state_preserves_last_resolved_status_brightness(self):
        source = (ROOT / 'src/leds/side_strip_handler.cpp').read_text()
        code = r'''
#include <cassert>
#include <cstdint>
#include <mutex>
#include <optional>
enum class SideStripState { unknown, printing, active, custom_color, dimmed, idle, off };
enum class LightState { deep_idle, idle, active, printing };
''' + function(source, 'static LightState light_state_for_strip_state(') + r'''
struct Controller {
    int calls=0;
    void set(int, uint32_t) { ++calls; }
} controller;
auto &controller_instance() { return controller; }
struct SideStripHandler {
    mutable std::mutex mutex;
    SideStripState state=SideStripState::unknown;
    LightState resolved_light_state=LightState::deep_idle;
    struct Custom { uint32_t transition_ms; };
    std::optional<Custom> custom_color=Custom{500};
    int get_color_for_state(SideStripState) { return 0; }
    LightState current_light_state() const;
    void change_state(SideStripState);
};
''' + function(
            source, 'LightState SideStripHandler::current_light_state() const'
        ) + '\n' + function(source,
                            'void SideStripHandler::change_state(') + r'''
int main() {
    SideStripHandler light;
    assert(light.current_light_state()==LightState::deep_idle);
    const SideStripState states[]={SideStripState::active,SideStripState::printing,
        SideStripState::idle,SideStripState::dimmed,SideStripState::off};
    for (auto state: states) {
        light.change_state(state);
        auto expected=light.current_light_state();
        for (int i=0;i<100;++i) {
            auto calls=controller.calls;
            light.state=SideStripState::unknown; // UI refresh between manager reads
            assert(light.current_light_state()==expected);
            assert(controller.calls==calls); // Getter must not reset animations
            light.change_state(state);
            assert(light.current_light_state()==expected);
        }
    }
    light.change_state(SideStripState::active);
    light.change_state(SideStripState::off); // Real timeout still dims/off normally
    assert(light.current_light_state()==LightState::deep_idle);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source_file = Path(directory) / 'test.cpp'
            binary = Path(directory) / 'test'
            source_file.write_text(code)
            subprocess.run([
                'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined',
                str(source_file), '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env={
                               **os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'
                           })


if __name__ == '__main__':
    unittest.main()

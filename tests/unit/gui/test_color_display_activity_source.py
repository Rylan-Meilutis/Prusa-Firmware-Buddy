"""Source contracts for GUI label ownership, LCD recovery and filter wiring.

These guard production call ordering, not physical LCD/fan operation.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


def read(path):
    return (ROOT / path).read_text()


class ColorDisplayActivitySource(unittest.TestCase):

    def test_load_and_change_pickers_own_temporary_profile_names(self):
        for path, classes in (('src/gui/screen/screen_preheat.cpp',
                               ('MI_LOAD_COLOR', 'MI_LOAD_MANUFACTURER')),
                              ('src/gui/screen_menu_filament_changeall.cpp',
                               ('MI_PRELOAD_COLOR',
                                'MI_PRELOAD_MANUFACTURER'))):
            source = read(path)
            for name in classes:
                with self.subTest(name=name):
                    body = source.split('class ' + name + ' final',
                                        1)[1].split('\n};', 1)[0]
                    self.assertIn('label_ {};', body)
                    self.assertIn('snprintf(label_.data(), label_.size()',
                                  body)
                    self.assertIn('MakeRAM(label_.data())', body)
                    self.assertNotIn('MakeRAM(name.data())', body)

    def test_loaded_manufacturer_also_copies_label(self):
        source = read('src/gui/screen/filament/screen_filaments_loaded.cpp')
        body = source.split('MI_ASSIGN_MANUFACTURER::MI_ASSIGN_MANUFACTURER(',
                            1)[1].split('void MI_ASSIGN_MANUFACTURER::click(',
                                        1)[0]
        self.assertIn('snprintf(label_.data(), label_.size()', body)
        self.assertIn('MakeRAM(label_.data())', body)

    def test_both_lcd_startup_sequences_clear_before_display_on(self):
        source = read('src/guiapi/src/ili9488.cpp')
        for name in ('startup_old_manufacturer', 'startup_new_manufacturer'):
            body = source.split('static void ' + name + '()',
                                1)[1].split('\n}', 1)[0]
            self.assertLess(body.index('ili9488_clear_full_black();'),
                            body.index('ili9488_cmd_dispon();'))

    def test_lcd_recovery_preserves_brightness_and_keeps_health_checks(self):
        source = read('src/guiapi/src/ili9488.cpp').split(
            'void ili9488_init(void)', 1)[1].split('void ili9488_done(void)',
                                                   1)[0]
        self.assertIn(
            'do_complete_lcd_reinit ? ili9488_config.brightness : 0xFF',
            source)
        self.assertIn('ili9488_brightness_set(restored_brightness)', source)
        checks = read('src/gui/display_hw_checks_xlcd.cpp')
        self.assertIn('display::is_reset_required()', checks)
        self.assertIn('Recovering LCD: display_check=%u', checks)

    def test_nested_filament_operations_only_begin_and_end_once(self):
        source = read('src/marlin_stubs/pause/M701_2.cpp')
        begin = source.split('filament_gcodes::InProgress::InProgress()',
                             1)[1].split(
                                 'filament_gcodes::InProgress::~InProgress()',
                                 1)[0]
        end = source.split('filament_gcodes::InProgress::~InProgress()',
                           1)[1].split('using namespace filament_gcodes;',
                                       1)[0]
        self.assertIn('if (lock == 1)', begin)
        self.assertIn('set_filament_activity(true)', begin)
        self.assertIn('if (lock == 0)', end)
        self.assertIn('set_filament_activity(false)', end)

    def test_activity_cycle_has_full_duration_and_can_be_stopped_manually(
            self):
        source = read('src/common/feature/chamber_filtration/chamber_filtration.cpp')
        setter = source.split(
            'void ChamberFiltration::set_filament_activity(',
            1)[1].split('bool ChamberFiltration::door_prompt_allowed()', 1)[0]
        self.assertIn('is_filtration_job_active(', setter)
        self.assertIn('start_post_print_filtration(false)', setter)
        self.assertIn('if (filament_activity_stopped_)', setter)
        self.assertIn(
            'filament_activity_active_ && !filament_activity_stopped_', source)
        self.assertIn('filament_activity_stopped_ = filament_activity_active_',
                      source)
        self.assertIn('.door_prompt_allowed = door_prompt_allowed_', source)
        self.assertIn('door_prompt_allowed_ = snapshot.door_prompt_allowed',
                      source)

    def test_door_prompt_requires_promptable_cycle(self):
        source = read('src/gui/guimain.cpp')
        body = source.split('void open_end_filtration_prompt_on_door_open()',
                            1)[1].split('} // namespace', 1)[0]
        self.assertIn('chamber_filtration().door_prompt_allowed()', body)
        self.assertIn('!filtering || !prompt_allowed', body)
        self.assertIn('&& prompt_allowed', body)


if __name__ == '__main__':
    unittest.main()

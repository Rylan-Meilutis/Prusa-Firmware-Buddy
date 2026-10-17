"""Guard firmware workflow wiring; these do not emulate touchscreen hardware."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


def read(path):
    return (ROOT / path).read_text()


class FilamentWorkflowSource(unittest.TestCase):

    def test_successful_load_commits_metadata_but_purge_does_not(self):
        source = read('src/marlin_stubs/pause/M701_2.cpp')
        helper = source.split('static void save_loaded_metadata(',
                              1)[1].split('\n}', 1)[0]
        self.assertIn(
            'filament_color::set_loaded(tool.to_raw(), filament::get_color_to_load())',
            helper)
        self.assertIn(
            'filament_manufacturer::set_loaded(tool.to_raw(), filament::get_manufacturer_to_load())',
            helper)
        self.assertLess(helper.index('filament_manufacturer::set_loaded'),
                        helper.index('notify_configuration'))
        load = source.split('void filament_gcodes::M701_load(',
                            1)[1].split('void filament_gcodes::M702_unload(',
                                        1)[0]
        success = load.split('if (load_unload(do_purge_only', 1)[1]
        self.assertIn(
            'if (!do_purge_only) {\n            save_loaded_metadata(virtual_tool);',
            success)

    def test_change_retains_prompt_color_and_commits_only_on_success(self):
        source = read('src/marlin_stubs/pause/M701_2.cpp').split(
            'void filament_gcodes::M1600_change_filament(', 1)[1]
        self.assertIn('color_to_be_loaded = filament::get_color_to_load();',
                      source)
        self.assertIn(
            'if (load_unload(Pause::LoadType::load, settings)) {\n        save_loaded_metadata(virtual_tool);',
            source)

    def test_autoload_does_not_discard_selected_color(self):
        source = read('src/marlin_stubs/pause/M701_2.cpp').split(
            'void filament_gcodes::M1701_autoload(',
            1)[1].split('void filament_gcodes::M1600_change_filament(', 1)[0]
        selected = source.split(
            'const FilamentType filament = preheat_ret.second;', 1)[1]
        self.assertNotIn('set_color_to_load(std::nullopt)', selected)
        self.assertIn('save_loaded_metadata(virtual_tool);', selected)

    def test_preload_color_screen_is_replaced_not_left_on_stack(self):
        source = read('src/gui/screen_menu_filament_changeall.cpp')
        color = source.split('class MI_PRELOAD_COLOR final',
                             1)[1].split('\n};', 1)[0]
        self.assertLess(
            color.index('Screens::Access()->Close();'),
            color.index(
                'Open(ScreenFactory::ScreenWithArg<ScreenPreloadManufacturer>')
        )
        for name in ('MI_PRELOAD_MANUFACTURER', 'MI_PRELOAD_NEW_MANUFACTURER'):
            body = source.split('class ' + name + ' final',
                                1)[1].split('\n};', 1)[0]
            self.assertEqual(body.count('Screens::Access()->Close();'), 1)

    def test_full_filament_lists_are_deferred_until_picker_click(self):
        source = read('src/gui/screen_menu_filament_changeall.cpp')
        self.assertIn('void MI_ActionSelect::click(IWindowMenu &menu)', source)
        self.assertIn('ConfigItem {} : config(), color, true)', source)
        self.assertIn('if (generate_list) {\n        generate_filament_list',
                      source)

    def test_new_job_restores_persisted_indx_detection(self):
        source = read('src/common/marlin_server.cpp').split(
            'buddy::extrusion_calibration::reset_job_results();', 1)[1]
        self.assertIn(
            'set_pressure_monitor_detection(\n            config_store().stuck_filament_detection.get(),\n            config_store().filament_movement_detection.get())',
            source[:650])
        report = read('src/marlin_stubs/M591.cpp')
        self.assertIn(
            '"Loadcell filament runout detection ", buddy::extrusion_calibration::pressure_monitor_presence_enabled()',
            report)

    def test_filter_cycle_is_available_in_control(self):
        source = read('src/gui/screen_menu_control.hpp')
        self.assertIn('MI_CHAMBER_FILTER_CYCLE,', source)
        self.assertIn('MI_CHAMBER_FILTRATION,', source)

    def test_backend_disable_and_snapshot_restore_close_filter_workflow(self):
        source = read('src/feature/chamber_filtration/chamber_filtration.cpp')
        backend = source.split(
            'void ChamberFiltration::set_backend(',
            1)[1].split('const char *ChamberFiltration::backend_name(', 1)[0]
        restore = source.split(
            'void ChamberFiltration::restore_snapshot(',
            1)[1].split('void ChamberFiltration::check_filter_expiration(',
                        1)[0]
        for body in (backend, restore):
            self.assertIn('const bool was_running = output_pwm_.value != 0;',
                          body)
            self.assertIn('notify_workflow("filtration", "closed"', body)


if __name__ == '__main__':
    unittest.main()

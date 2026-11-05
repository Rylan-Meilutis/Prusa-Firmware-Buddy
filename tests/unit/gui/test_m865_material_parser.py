"""Tie M865's commit boundary to the executable real-parser regression."""
import unittest
import test_serial_pause_recovery as fixture


class M865MaterialParser(unittest.TestCase):

    def test_m865_uses_tested_parser_and_rejects_partial_profile_writes(self):
        source = (fixture.ROOT / 'src/marlin_stubs/M865.cpp').read_text()
        call = 'if (!buddy::m865_material::apply_base_option(p, params)) {'
        self.assertIn('#include <m865_material.hpp>', source)
        self.assertNotIn('Name base_preset_name', source)
        block = fixture.function(source, call)
        self.assertIn('return;', block)
        self.assertLess(source.index('params = {};'), source.index(call))
        self.assertLess(source.index(call),
                        source.index('filament_type.set_parameters(params)'))
        self.assertLess(source.index(call),
                        source.index('config_store().set_filament_type('))


if __name__ == '__main__':
    unittest.main()

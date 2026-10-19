"""Release-floor regression using the real firmware heap/linker symbols."""
import importlib.util
import pathlib
import sys
import unittest


class IndxRuntimeMemoryTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        root = pathlib.Path(__file__).resolve().parents[3]
        spec = importlib.util.spec_from_file_location("rme_build_budget",
                                                      root / "build.py")
        cls.build = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.build
        spec.loader.exec_module(cls.build)

    def test_published_sram_layout_is_rejected(self):
        # Symbols from e2a931ded, the release implicated by the new heap report.
        with self.assertRaisesRegex(ValueError, "must not consume"):
            self.build.check_indx_runtime_memory(
                "20030000 A _estack\n200272f0 B end\n"
                "10007b74 00005d00 b _ZL10task_stack\n"
                "20019728 00003000 b _ZL17marlin_stack_sram\n")

    def test_heap_floor_is_independent_of_stack_symbol(self):
        with self.assertRaisesRegex(ValueError, "below 44 KiB"):
            self.build.check_indx_runtime_memory(
                "20030000 A _estack\n200272f0 B end\n"
                "10007b74 00007d00 b _ZL10task_stack\n")

    def test_restored_heap_budget_passes(self):
        self.assertEqual(
            46352,
            self.build.check_indx_runtime_memory(
                "20030000 A _estack\n200242f0 B end\n"
                "10007b74 00007d00 b _ZL10task_stack\n"))

    def test_missing_symbols_and_ccm_overflow_fail_closed(self):
        with self.assertRaises(KeyError):
            self.build.check_indx_runtime_memory("")
        with self.assertRaisesRegex(ValueError, "entirely in CCM"):
            self.build.check_indx_runtime_memory(
                "20030000 A _estack\n200242f0 B end\n"
                "10007b74 00009000 b _ZL10task_stack\n")

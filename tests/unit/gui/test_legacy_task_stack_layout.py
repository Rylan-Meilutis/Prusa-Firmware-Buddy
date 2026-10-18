"""Preserve task sizes when making room for Marlin on old chamber targets."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


class LegacyTaskStackLayout(unittest.TestCase):
    def test_only_connect_stack_moves_and_allocation_stays_static(self):
        source = (ROOT / "src/buddy/main.cpp").read_text()
        connect = source.split("static uint32_t connect_stack_sram[2336];", 1)[1].split("#endif", 1)[0]
        self.assertIn("static StaticTask_t connect_control_sram", connect)
        self.assertIn("osThreadStaticDef(connectTask", connect)
        self.assertIn("TASK_PRIORITY_CONNECT, 0, 2336, connect_stack_sram", connect)
        self.assertIn("#else", connect)
        self.assertIn("osThreadCCMDef(connectTask", connect)
        self.assertIn("osThreadCCMDef(defaultTask, StartDefaultTask, TASK_PRIORITY_DEFAULT_TASK, 0, buddy::task_stack_requirements::marlin_words)", source)
        self.assertIn("osThreadCCMDef(acFaultTask", source)
        self.assertIn("osThreadCCMDef(measurementTask", source)

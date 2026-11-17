"""Official 8.0.2 supports the combined existing 8.0.0/8.0.1 RME matrix."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location(
    "rme_support", ROOT / "utils/rme_firmware_support.py")
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)


class Support802(unittest.TestCase):

    def test_combined_official_machine_families(self):
        self.assertEqual(
            support.SUPPORTED["8.0.2"],
            support.SUPPORTED["8.0.0"] | support.SUPPORTED["8.0.1"])
        self.assertEqual(len(support.SUPPORTED["8.0.2"]), 15)

    def test_unknown_versions_still_fail_closed(self):
        self.assertNotIn("8.99.99", support.SUPPORTED)

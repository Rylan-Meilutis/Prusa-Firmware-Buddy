"""Release identity matches actual executable bytes, never a staged candidate."""
import hashlib
import importlib.util
import re
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location(
    "manifest", ROOT / "utils/rme_firmware_manifest.py")
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)


class IdentityTests(unittest.TestCase):

    def test_built_application_bounds_match_bbf_payload(self):
        build = ROOT / "build/coreone_indx_release_boot"
        if not (build / "firmware.map").exists():
            self.skipTest("INDX build unavailable")
        mapping = (build / "firmware.map").read_text()
        addresses = [
            int(
                re.search(r"(0x[0-9a-f]+)\s+__rme_application_" + name,
                          mapping)[1], 16) for name in ("start", "end")
        ]
        binary = (build / "firmware.bin").read_bytes()
        bbf = (build / "firmware.bbf").read_bytes()
        self.assertEqual(addresses[1] - addresses[0], len(binary))
        self.assertEqual(int.from_bytes(bbf[96:100], "little"), len(binary))
        self.assertEqual(bbf[576:576 + len(binary)], binary)

    def test_manifest_hashes_payload_and_whole_file_separately(self):
        payload = b"actual executable bytes" * 10
        header = len(payload).to_bytes(4, "little") + bytes(476)
        data = bytes(64) + hashlib.sha256(
            header + payload).digest() + header + payload + b"resources"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "coreone_indx_6.10.1-RME.bbf"
            path.write_bytes(data)
            result = manifest.entry(path)
            self.assertEqual(result["application_size"], len(payload))
            self.assertEqual(result["application_sha256"],
                             hashlib.sha256(payload).hexdigest())
            self.assertEqual(result["sha256"],
                             hashlib.sha256(data).hexdigest())
            path.write_bytes(data[:100])
            with self.assertRaises(ValueError):
                manifest.entry(path)
            path.write_bytes(data[:64] + bytes(32) + data[96:])
            with self.assertRaises(ValueError):
                manifest.entry(path)

    def test_running_hash_uses_linked_flash_bounds_not_usb(self):
        source = (ROOT / "src/marlin_stubs/rme_file_service.cpp").read_text()
        section = source.split('action_is(action, "RUNNING")')[1].split(
            'action_is(action, "QUERY")')[0]
        self.assertIn("__rme_application_start", section)
        self.assertIn("__rme_application_end", section)
        self.assertIn("mbedtls_sha256_ret", section)
        self.assertIn("!marlin_server::printer_idle()", section)
        self.assertNotIn("fopen", section)
        linker = (ROOT /
                  "src/device/stm32f4/linker/stm32f4_platform.ld").read_text()
        self.assertIn(
            "__rme_application_end = LOADADDR(.data) + SIZEOF(.data)", linker)


if __name__ == "__main__":
    unittest.main()

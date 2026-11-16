"""Check actual ARM code for context-preserving hardware fault entry.

This verifies entry instructions, not flash persistence or a hardware crash.
"""
import pathlib
import re
import subprocess
import tempfile
import unittest


class FaultCaptureEntryTests(unittest.TestCase):

    def test_hardware_faults_tail_branch_without_touching_fault_context(self):
        root = pathlib.Path(__file__).resolve().parents[3]
        toolchain = root / ".dependencies/gcc-arm-none-eabi-13.3.1/bin"
        compiler = toolchain / "arm-none-eabi-g++"
        objdump = toolchain / "arm-none-eabi-objdump"
        if not compiler.exists() or not objdump.exists():
            self.skipTest("ARM toolchain unavailable")
        source = (root /
                  "src/device/stm32f4/interrupts_common.cpp").read_text()
        names = ("MemManage_Handler", "BusFault_Handler", "UsageFault_Handler")
        functions = []
        for name in names:
            match = re.search(
                r'extern "C" __attribute__\(\(naked\)\) void ' + name +
                r'\(\) \{[^}]*\}', source)
            self.assertIsNotNone(match, name)
            functions.append(match.group())
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "faults.cpp"
            cpp.write_text("\n".join(functions))
            for optimization in ("-O0", "-Os", "-O2"):
                obj = pathlib.Path(directory) / "faults.o"
                subprocess.run([
                    str(compiler), "-mcpu=cortex-m4", "-mthumb", optimization,
                    "-c",
                    str(cpp), "-o",
                    str(obj)
                ],
                               check=True,
                               capture_output=True)
                assembly = subprocess.check_output(
                    [str(objdump), "-dr", str(obj)], text=True)
                for name in names:
                    body = assembly.split("<" + name + ">:",
                                          1)[1].split("\n\n", 1)[0]
                    instructions = re.findall(
                        r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{4}\s+)+\s*([^\n]+)',
                        body, re.MULTILINE)
                    self.assertTrue(instructions, body)
                    self.assertRegex(instructions[0],
                                     r'^b\.w\s+.*<HardFault_Handler>')
                    self.assertIn("R_ARM_THM_JUMP24\tHardFault_Handler", body)
                    # GCC may append unreachable padding/trap instructions, but
                    # no executed instruction precedes the unconditional jump.
                    self.assertNotRegex(body, r'\b(push|pop|bl|blx)\b')


if __name__ == "__main__":
    unittest.main()

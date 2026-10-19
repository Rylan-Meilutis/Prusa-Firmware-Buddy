"""Execute the sensor bootstrap deadlines without printer hardware."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class ToolSensorDeadlineTests(unittest.TestCase):

    def test_verify_flash_and_total_deadlines(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        code = r'''
#include <cassert>
#include "tool_sensor_bootstrap_policy.hpp"
using buddy::puppies::ToolSensorBootstrapDeadline;
int main() {
    ToolSensorBootstrapDeadline verify(0);
    assert(!verify.stalled(14999, false, 0));
    assert(verify.stalled(15000, false, 0));
    ToolSensorBootstrapDeadline flash(0);
    assert(!flash.stalled(1, true, 0));
    assert(!flash.stalled(59000, true, 10));
    assert(!flash.stalled(118999, true, 10));
    assert(flash.stalled(119000, true, 10));
    ToolSensorBootstrapDeadline wrap(UINT32_MAX - 1000);
    assert(!wrap.stalled(13998, false, 0));
    assert(wrap.stalled(13999, false, 0));
    ToolSensorBootstrapDeadline looping(0);
    for (uint32_t now = 0; now < 300000; now += 1000) {
        assert(!looping.stalled(now, (now / 1000) % 2, 1));
    }
    assert(looping.stalled(300000, true, 2));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run([
                compiler, "-std=c++17", "-I",
                str(root / "src/common"),
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

"""Exercise production readiness wait with invalid startup samples and retries."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class LoadcellReadinessTests(unittest.TestCase):

    def test_undefined_samples_cannot_satisfy_probe_readiness(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = Path(__file__).resolve().parents[3]
        source = (root / "src/common/loadcell.cpp").read_text()
        begin = source.index("bool Loadcell::wait_for_fresh_sample(")
        wait = source[begin:source.index("\n}\n", begin) + 2]
        code = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
uint32_t now;
int waits, valid_at;
bool abort_wait;
uint32_t ticks_us() { return now; }
int32_t ticks_diff(uint32_t a, uint32_t b) { return int32_t(a - b); }
struct Loadcell {
    std::atomic<uint32_t> last_sample_time_us{0};
    int undefinedCnt = -6;
    bool probe_should_abort() const { return abort_wait; }
    bool wait_for_fresh_sample(uint32_t, uint32_t);
} cell;
void idle(bool) {
    ++waits;
    now += 1000;
    // Like ProcessSample(), even undefined incoming samples publish a timestamp.
    cell.last_sample_time_us.store(now);
    if (valid_at >= 0 && waits >= valid_at) cell.undefinedCnt = 0;
}
''' + wait + r'''
void reset() {
    now = 100000;
    waits = 0;
    valid_at = -1;
    abort_wait = false;
    cell.undefinedCnt = -6;
    cell.last_sample_time_us.store(now);
}
int main() {
    reset(); // first run after startup: recent timestamp but no valid sample
    valid_at = 3;
    assert(cell.wait_for_fresh_sample(5000, 2000));
    assert(waits == 3);
    // Retry after valid samples are arriving must also work.
    waits = 0;
    assert(cell.wait_for_fresh_sample(5000, 2000));
    assert(waits == 0);
    reset(); // continuous invalid samples cannot bypass the timeout
    assert(!cell.wait_for_fresh_sample(5000, 2000));
    assert(waits == 6);
    reset(); // prior valid sample followed by undefined data is not ready either
    cell.undefinedCnt = 1;
    assert(!cell.wait_for_fresh_sample(5000, 2000));
    reset(); // cancellation must not be cleared to get readiness
    abort_wait = true;
    valid_at = 0;
    assert(!cell.wait_for_fresh_sample(5000, 2000));
    assert(waits == 0 && abort_wait);
    reset(); // a stale valid sample must await fresh data
    cell.undefinedCnt = 0;
    cell.last_sample_time_us.store(now - 10000);
    assert(cell.wait_for_fresh_sample(5000, 2000));
    assert(waits == 1);
    reset(); // timestamp wrap does not falsely reject fresh valid data
    now = 100;
    cell.last_sample_time_us.store(UINT32_MAX - 99);
    cell.undefinedCnt = 0;
    assert(cell.wait_for_fresh_sample(5000, 2000));
    assert(waits == 0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            executable = Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run(
                [compiler, "-std=c++20",
                 str(cpp), "-o",
                 str(executable)],
                check=True,
                capture_output=True)
            subprocess.run([str(executable)],
                           check=True,
                           capture_output=True,
                           cwd=directory)


if __name__ == "__main__":
    unittest.main()

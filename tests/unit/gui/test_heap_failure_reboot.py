"""Exercise production heap-growth failure handling across simulated RAM resets.

This tests _sbrk_r, not newlib fragmentation or a full-board reboot.
"""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class HeapFailureRebootTests(unittest.TestCase):

    def test_failed_growth_unmasks_interrupts_and_does_not_poison_next_boot(
            self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/common/heap.cpp").read_text()
        start = source.index("void *_sbrk_r(")
        end = source.index("\nvoid *sbrk(", start)
        code = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <stdexcept>
struct _reent {};
using UBaseType_t = unsigned;
alignas(16) char ram[46336];
char *cursor = ram;
char *&current_heap_end() { return cursor; }
char *heap_end() { return ram + sizeof(ram); }
int critical_depth = 0;
#define ENTER_CRITICAL_SECTION(saved) do { (saved) = critical_depth++; } while (false)
#define EXIT_CRITICAL_SECTION(saved) do { critical_depth = (saved); } while (false)
std::atomic<int> fallible_request_for {0};
int current_task = 1;
int xTaskGetCurrentTaskHandle() { return current_task; }
bool retained_heap_message = false;
void vApplicationMallocFailedHook() {
    assert(critical_depth == 0);
    retained_heap_message = true;
    throw std::runtime_error("Out of heap memory");
}
''' + source[start:end] + r'''
int main() {
    _reent context;
    for (int restart = 0; restart < 1000; ++restart) {
        // Reset volatile RAM state, preserving the error-message flag.
        cursor = ram; fallible_request_for = 0; current_task = 1;
        assert(_sbrk_r(&context, 0) == ram && cursor == ram);
        assert(_sbrk_r(&context, sizeof(ram)) == ram);
        assert(cursor == heap_end() && critical_depth == 0);
        bool failed = false;
        try { (void)_sbrk_r(&context, 1); }
        catch (const std::runtime_error &) { failed = true; }
        assert(failed && retained_heap_message);
        assert(cursor == heap_end() && critical_depth == 0);
        // A fallible request by this task returns failure without a fatal hook.
        retained_heap_message = false;
        fallible_request_for = current_task;
        assert(_sbrk_r(&context, 1) == (void *)-1);
        assert(!retained_heap_message && critical_depth == 0);
        // Another task's fallible request cannot suppress our fatal failure.
        fallible_request_for = current_task + 1;
        failed = false;
        try { (void)_sbrk_r(&context, 1); }
        catch (const std::runtime_error &) { failed = true; }
        assert(failed && critical_depth == 0);
        // Failed growth left the allocator cursor unchanged; shrinking works.
        fallible_request_for = 0;
        assert(_sbrk_r(&context, -16) == heap_end());
        assert(_sbrk_r(&context, 16) == heap_end() - 16);
        assert(cursor == heap_end() && critical_depth == 0);
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            executable = pathlib.Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run(
                [compiler, "-std=c++17",
                 str(cpp), "-o",
                 str(executable)],
                check=True)
            subprocess.run([str(executable)], check=True, timeout=5)

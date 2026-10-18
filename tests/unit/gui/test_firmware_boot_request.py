"""Execute the production handoff functions against retained bootloader RAM."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class FirmwareBootRequestTests(unittest.TestCase):

    def test_retained_request_is_not_armed_in_new_application(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/common/data_exchange.cpp").read_text()
        functions = source[source.index("void set_reflash_bbf_sfn("):source.
                           index("bool has_apendix(")]
        runtime_flag = "static bool reflash_requested_this_boot = false;"
        self.assertIn(runtime_flag, source)
        harness = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
enum class FwAutoUpdate { off, specified };
struct DataExchange { FwAutoUpdate fw_update_flag; char bbf_sfn[13]; };
DataExchange ram_data_exchange { FwAutoUpdate::specified, "FWUPD.RME" };
FwAutoUpdate get_auto_update_flag() { return ram_data_exchange.fw_update_flag; }
#define strlcpy test_strlcpy
void test_strlcpy(char *dst, const char *src, size_t size) { snprintf(dst, size, "%s", src); }
''' + runtime_flag + functions + r'''
int main() {
    // Bootloader returned with the previous request still in retained RAM.
    assert(!is_reflash_bbf_sfn("FWUPD.RME"));
    set_reflash_bbf_sfn("FWUPD.RME");
    assert(is_reflash_bbf_sfn("FWUPD.RME"));
    assert(!is_reflash_bbf_sfn("other.bbf"));
    assert(!is_reflash_bbf_sfn(nullptr));
    clear_reflash_bbf_sfn();
    assert(!is_reflash_bbf_sfn("FWUPD.RME"));
    assert(ram_data_exchange.fw_update_flag == FwAutoUpdate::off);
    assert(ram_data_exchange.bbf_sfn[0] == 0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            cpp.write_text(harness)
            subprocess.run(
                [compiler, "-std=c++17",
                 str(cpp), "-o",
                 str(binary)],
                check=True)
            subprocess.run([str(binary)], check=True)

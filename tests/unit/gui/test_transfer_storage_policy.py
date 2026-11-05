"""Execute the production partial-file factories without filesystem hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class TransferStoragePolicy(unittest.TestCase):

    def test_only_available_storage_backends_are_opened(self):
        source = (ROOT / "src/transfers/partial_file.cpp").read_text()
        factories = function(source,
                             "PartialFile::Result PartialFile::create(")
        factories += function(source, "PartialFile::Result PartialFile::open(")
        code = r'''
#include <cassert>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#define HAS_INTERNAL_STORAGE_FLASH() HAS_STORAGE
namespace storage {
enum class Type {usb, internal};
std::optional<Type> type(const char *path) {
 if(std::string_view(path).starts_with("/usb/")) return Type::usb;
#if HAS_INTERNAL_STORAGE_FLASH()
 if(std::string_view(path).starts_with("/internal/")) return Type::internal;
#endif
 return {};
}
}
int opens=0;
const char *last_mode=nullptr;
using unique_file_ptr=std::unique_ptr<int>;
unique_file_ptr open_partial_file(const char *, const char *mode) {
 ++opens; last_mode=mode; return std::make_unique<int>(1);
}
struct PartialFile {
 struct State {};
 using Result=std::variant<const char *,int>;
 static Result create(const char *, size_t);
 static Result open(const char *, State);
};
struct UsbPartialFile {
 static PartialFile::Result create(const char *, unique_file_ptr, size_t) {return 1;}
 static PartialFile::Result open(const char *, unique_file_ptr, PartialFile::State) {return 2;}
};
#if HAS_INTERNAL_STORAGE_FLASH()
struct PosixPartialFile {
 static PartialFile::Result create(const char *, unique_file_ptr, size_t) {return 3;}
 static PartialFile::Result open(const char *, unique_file_ptr, PartialFile::State) {return 4;}
};
#endif
''' + factories + r'''
int main() {
 assert(std::get<int>(PartialFile::create("/usb/job",100))==1);
 assert(!strcmp(last_mode,"wb+"));
 assert(std::get<int>(PartialFile::open("/usb/job",{}))==2);
 assert(!strcmp(last_mode,"rb"));
 for(const char *path:{"/system/secret","/unknown/job"}) {
  int before=opens;
  assert(std::holds_alternative<const char *>(PartialFile::create(path,100)));
  assert(std::holds_alternative<const char *>(PartialFile::open(path,{})));
  assert(opens==before);
 }
#if HAS_INTERNAL_STORAGE_FLASH()
 assert(std::get<int>(PartialFile::create("/internal/job",100))==3);
 assert(std::get<int>(PartialFile::open("/internal/job",{}))==4);
 assert(!strcmp(last_mode,"rb+"));
#else
 int before=opens;
 assert(std::holds_alternative<const char *>(PartialFile::create("/internal/job",100)));
 assert(std::holds_alternative<const char *>(PartialFile::open("/internal/job",{})));
 assert(opens==before);
#endif
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            cpp.write_text(code)
            for internal in (0, 1):
                executable = Path(directory) / ("test" + str(internal))
                subprocess.run([
                    "c++", "-std=c++20", "-DHAS_STORAGE=" + str(internal),
                    str(cpp), "-o",
                    str(executable)
                ],
                               check=True)
                subprocess.run([str(executable)], check=True, timeout=5)

    def test_upstream_retraction_volume_is_signed(self):
        source = (
            ROOT /
            "src/common/tool/hotend/hotend/local_hotend.cpp").read_text()
        line = next(line for line in source.splitlines()
                    if "const float e_volume_delta =" in line)
        code = r'''
#include <cassert>
#include <cstdint>
constexpr int E_AXIS=0;
struct {float mm_per_step[1]={.01f};} planner;
float delta(uint32_t e_position,uint32_t last_e_position_,bool is_current_tool) {
 constexpr float distance_to_volume_per_second=1;
''' + line + r'''
 return e_volume_delta;
}
int main() {
 assert(delta(900,1000,true)<0);
 assert(delta(1000,900,true)>0);
 assert(delta(900,1000,false)==0);
 assert(delta(4,UINT32_MAX-5,true)>0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "heater.cpp"
            cpp.write_text(code)
            executable = Path(directory) / "heater"
            subprocess.run(
                ["c++", "-std=c++20",
                 str(cpp), "-o",
                 str(executable)],
                check=True)
            subprocess.run([str(executable)], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()

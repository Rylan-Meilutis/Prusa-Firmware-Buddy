"""Execute production BBF discovery/digest matching on a temporary USB tree."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class FirmwareResourceSourceTests(unittest.TestCase):

    def test_confirmed_bbf_and_legacy_neutral_handoff_supply_resources(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/resources/bootstrap.cpp").read_text()
        predicates = source[
            source.index("static bool has_bbf_suffix("):source.
            index("[[nodiscard]] static bool remove_directory_recursive(")]
        discovery = source[
            source.index("static bool find_suitable_bbf_file("):source.
            index("[[nodiscard]] static bool extract_and_hash(")]
        harness = r'''
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <memory>
#include <string>
std::string base;
std::string mapped(const char *p) { return base + std::string(p).substr(4); }
FILE *open_usb(const char *p,const char *mode) { return fopen(mapped(p).c_str(), mode); }
struct Directory {
 DIR *dir;
 Directory(const char *p):dir(opendir(mapped(p).c_str())) {}
 ~Directory() { if(dir) closedir(dir); }
 explicit operator bool() const { return dir; }
 dirent *read() { return readdir(dir); }
};
struct Path {
 std::string value;
 void set(const char *p) { value=p; }
 void push(const char *p) { value+='/'; value+=p; }
 const char *get() const { return value.c_str(); }
};
struct Close { void operator()(FILE *f) const { fclose(f); } };
using unique_file_ptr=std::unique_ptr<FILE,Close>;
namespace buddy {
namespace resources {
 struct Revision {
  std::array<unsigned char,4> hash;
  bool operator==(const Revision &r) const { return hash==r.hash; }
 };
}
namespace bbf {
 enum class TLVType { RESOURCES_TARBALL_DIGEST, BOOTLOADER_TARBALL_DIGEST,
                      RESOURCES_TARBALL, BOOTLOADER_TARBALL };
 bool seek_to_tlv_entry(FILE *f,TLVType type,unsigned &size) {
  size=4;
  return fseek(f,type==TLVType::RESOURCES_TARBALL_DIGEST ? 0 : 4,SEEK_SET)==0;
 }
}
}
#define log_debug(...) ((void)0)
#define log_info(...) ((void)0)
#define log_warning(...) ((void)0)
#define log_error(...) ((void)0)
#define fopen open_usb
''' + predicates + discovery + r'''
void put(const char *name, const char *hash="goodbad!") {
 FILE *f=fopen(name,"wb"); assert(f);
 assert(fwrite(hash,1,strlen(hash),f)==strlen(hash)); fclose(f);
}
void fresh() { std::filesystem::remove_all(base); std::filesystem::create_directory(base); }
int main(int argc,char **argv) {
 assert(argc==2); base=argv[1]; Path path;
 buddy::resources::Revision revision{{'g','o','o','d'}};
 buddy::bbf::TLVType entry;
 for(const char *name : {"FWUPD.BBF", "FWUPD.RME", "fwupd.rme", "original.bbf"}) {
  fresh(); std::string p="/usb/"; p+=name; put(p.c_str());
  assert(find_suitable_bbf_file(revision,path,entry));
  assert(path.value==p && entry==buddy::bbf::TLVType::RESOURCES_TARBALL);
 }
 for(const char *name : {"other.rme", "FWUPD.RME.rme-part", "FWUPD.RME.rme-meta", "FWUPD.RME.tmp", "FWUPD.UI"}) {
  fresh(); std::string p="/usb/"; p+=name; put(p.c_str());
  assert(!find_suitable_bbf_file(revision,path,entry));
 }
 // A matching filename is not authority to install a mismatching payload.
 for(const char *hash : {"", "bad", "bad!bad!"}) {
  fresh(); put("/usb/FWUPD.RME",hash);
  assert(!find_suitable_bbf_file(revision,path,entry));
 }
 fresh(); put("/usb/FWUPD.RME","bad!good");
 assert(find_suitable_bbf_file(revision,path,entry));
 assert(entry==buddy::bbf::TLVType::BOOTLOADER_TARBALL);
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
            subprocess.run([str(binary),
                            str(pathlib.Path(directory) / "usb")],
                           check=True)
        # Resource copying finishes before Marlin's cleanup worker can start.
        startup = (root / "src/buddy/main.cpp").read_text()
        self.assertLess(startup.index("    resources_update();"),
                        startup.index('create_task("marlin"'))
        # Matching the archive's digest remains mandatory after extraction.
        self.assertIn("if (revision.hash != current_hash)", source)


if __name__ == "__main__":
    unittest.main()

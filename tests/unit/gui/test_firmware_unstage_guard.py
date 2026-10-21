"""Execute the production conditional-delete guard with injected storage state."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class FirmwareUnstageGuardTests(unittest.TestCase):

    def test_guard_never_deletes_replaced_or_unverified_candidates(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/marlin_stubs/rme_file_service.cpp").read_text()
        start = source.index(
            '        const auto expected_sha = value(command, "sha256");')
        end = source.index("        stop_firmware_validation();", start)
        guard = source[start:end]
        harness = r'''
#include <array>
#include <cassert>
#include <optional>
#include <string_view>
#include <sys/stat.h>
#include "rme_firmware_status.hpp"
using namespace std::literals;
std::optional<std::string_view> sha, size_arg;
std::optional<unsigned> size;
bool present = true, verified = true, regular = true;
unsigned disk_size = 100;
unsigned deleted = 0;
std::array<uint8_t, 32> stored_digest{};
struct { bool file = false, suspended = false; } upload;
namespace transfers { struct Monitor { enum class Outcome { Stopped }; }; }
struct Slot { void done(transfers::Monitor::Outcome) {} };
std::optional<std::string_view> value(int, const char *key) {
    return std::string_view(key) == "sha256" ? sha : size_arg;
}
std::optional<unsigned> number(int, const char *) { return size; }
bool parse_sha256(std::string_view text, std::array<uint8_t, 32> &out) {
    out.fill(1); return text == "valid";
}
int fake_stat(const char *, struct stat *out) {
    out->st_size = disk_size; out->st_mode = regular ? S_IFREG : S_IFDIR;
    return present ? 0 : -1;
}
bool read_verified_firmware_metadata(unsigned, rme_firmware_status::VerifiedMetadata &out) {
    out.sha256 = stored_digest; return verified;
}
#define stat(path, out) fake_stat(path, out)
#define SERIAL_ECHOLNPGM(message) (void)0
const char *firmware_candidate_path = "/usb/FWUPD.RME";
bool attempt() {
    int command = 0; Slot holder; Slot *slot = &holder;
''' + guard + r'''
    ++deleted; return false;
}
int main() {
    sha = "valid"; size_arg = "100"; size = 100; stored_digest.fill(1);
    assert(!attempt() && deleted == 1);
    sha = "bad"; assert(attempt() && deleted == 1); sha = "valid";
    size.reset(); assert(attempt() && deleted == 1); size = 100;
    sha.reset(); assert(attempt() && deleted == 1); sha = "valid";
    disk_size = 99; assert(attempt() && deleted == 1); disk_size = 100;
    stored_digest.fill(2); assert(attempt() && deleted == 1); stored_digest.fill(1);
    present = false; assert(attempt() && deleted == 1); present = true;
    regular = false; assert(attempt() && deleted == 1); regular = true;
    verified = false; assert(attempt() && deleted == 1); verified = true;
    upload.file = true; assert(attempt() && deleted == 1); upload.file = false;
    upload.suspended = true; assert(attempt() && deleted == 1); upload.suspended = false;
    size = 0; assert(attempt() && deleted == 1); size = 100;
    // Existing operator-requested, unguarded UNSTAGE retains its API.
    sha.reset(); size_arg.reset(); size.reset();
    assert(!attempt() && deleted == 2);
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary)
            (path / "guard.cpp").write_text(harness)
            subprocess.run([
                compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I",
                str(root / "src/common"),
                str(path / "guard.cpp"), "-o",
                str(path / "guard")
            ],
                           check=True,
                           capture_output=True)
            subprocess.run([str(path / "guard")], check=True)

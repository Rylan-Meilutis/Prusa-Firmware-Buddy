"""Run the actual server cleanup block against transient filesystem faults."""
import pathlib
import os
import shutil
import subprocess
import tempfile
import unittest


class FirmwareCleanupRetryTests(unittest.TestCase):

    def test_server_retries_transient_io_and_preserves_unflashed_candidate(
            self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/common/marlin_server.cpp").read_text()
        start = source.index(
            "    const bool media = usb_host::is_media_inserted();")
        end = source.index("    if (marlin_vars().media_inserted != media)",
                           start)
        block = source[start:end]
        harness = r'''
#include <cassert>
#include <cerrno>
#include <optional>
#include <set>
#include <string>
#include <sys/stat.h>
#include "firmware_cleanup_gate.hpp"
#include "firmware_update_handoff.hpp"
#include "firmware_cleanup_receipt.hpp"
static uint32_t now_ms = 0;
static bool inserted = true, busy = false, armed = false;
static int stat_failures = 1, unlink_failures = 1, inspections = 0;
static std::string fail_path = "/usb/FWUPD.RME";
static std::set<std::string> files;
uint32_t ticks_ms() { return now_ms; }
namespace usb_host { bool is_media_inserted() { return inserted; } }
namespace data_exchange { bool is_reflash_bbf_sfn(const char *) { return armed; } }
namespace transfers {
struct Monitor {
    enum class Type { Link };
    enum class Outcome { Finished, ErrorStorage };
    struct Slot { void done(Outcome) {} };
    std::optional<int> id() { return busy ? std::optional<int>(1) : std::nullopt; }
    std::optional<Slot> allocate(Type, const char *, int) {
        return busy ? std::nullopt : std::optional<Slot>(Slot{});
    }
    static Monitor instance;
};
Monitor Monitor::instance;
}
int fake_stat(const char *path, struct stat *result) {
    ++inspections;
    if (stat_failures > 0) { --stat_failures; errno = EBUSY; return -1; }
    if (!files.count(path)) { errno = ENOENT; return -1; }
    result->st_mode = S_IFREG;
    return 0;
}
int fake_remove(const char *path) {
    if (std::string(path) == fail_path && unlink_failures > 0) {
        --unlink_failures; errno = EBUSY; return -1;
    }
    if (!files.erase(path)) { errno = ENOENT; return -1; }
    return 0;
}
#define stat(path, result) fake_stat(path, result)
#define remove fake_remove
namespace firmware_update_handoff {
ReceiptState cleanup_receipt_state() {
    return files.count("/usb/FWUPD.CLN") ? ReceiptState::valid : ReceiptState::missing;
}
bool remove_cleanup_receipt() {
    for (auto path : {"/usb/FWUPD.TCL", "/usb/FWUPD.CLN"}) {
        errno = 0;
        if (remove(path) != 0 && errno != ENOENT) return false;
    }
    return true;
}
}
void poll() {
''' + block + r'''
}
int main() {
    files = {"/usb/FWUPD.UI", "/usb/FWUPD.RME", "/usb/FWUPD.RME.rme-verified"};
    poll(); // First USB access is temporarily not ready.
    assert(inspections == 1 && files.count("/usb/FWUPD.RME"));
    now_ms = 999; poll(); assert(inspections == 1);
    now_ms = 1000; poll(); // First unlink fails; retain marker for retry.
    assert(files.count("/usb/FWUPD.RME") && files.count("/usb/FWUPD.UI"));
    now_ms = 2000; busy = true; poll(); assert(inspections == 2);
    busy = false; poll(); assert(files.empty());
    now_ms = 3000; poll(); assert(inspections == 3); // No repeated I/O.
    inserted = false; poll(); inserted = true;
    files = {"/usb/FWUPD.RME"}; poll();
    assert(files.count("/usb/FWUPD.RME")); // No handoff marker: merely staged.
    inserted = false; poll(); inserted = true;
    files.insert("/usb/FWUPD.UI"); armed = true; poll();
    assert(files.count("/usb/FWUPD.RME")); // Must survive pre-reset handoff.

    // A successful ENOENT is not a lifetime guarantee that a mount's metadata
    // is fully visible. Reproduce a marker becoming visible after that check,
    // without requiring physical removal/reinsertion of the USB drive.
    inserted = false; poll(); inserted = true; armed = false;
    files = {"/usb/FWUPD.RME", "/usb/FWUPD.BBF"};
    now_ms = 10000; poll();
    assert(files.size() == 2); // Merely staged: absolutely no deletion.
    const int before = inspections;
    files.insert("/usb/FWUPD.UI");
    now_ms = 10001; poll(); assert(inspections == before); // Rate limited.
    now_ms = 40000; poll();
    assert(files.empty()); // Both protected and legacy image must be consumed.

    // The reported failure: the primary marker is permanently absent.
    inserted = false; poll(); inserted = true;
    files = {"/usb/FWUPD.RME", "/usb/FWUPD.CLN"};
    armed = false;
    now_ms += 40000; poll(); assert(files.empty());

    // Every unlink failure must retain the marker and retry. In particular,
    // removing the .RME image must not stop retries for a leftover .BBF.
    for (const char *path : {"/usb/FWUPD.RME", "/usb/FWUPD.BBF",
             "/usb/FWUPD.RME.rme-verified", "/usb/FWUPD.RME.rme-verified-tmp",
             "/usb/FWUPD.UI"}) {
        inserted = false; poll(); inserted = true;
        files = {"/usb/FWUPD.RME", "/usb/FWUPD.BBF", "/usb/FWUPD.UI",
                 "/usb/FWUPD.RME.rme-verified", "/usb/FWUPD.RME.rme-verified-tmp"};
        fail_path = path; unlink_failures = 1; now_ms += 40000;
        poll(); assert(files.count("/usb/FWUPD.UI"));
        now_ms += 1000; poll(); assert(files.empty());
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            cpp.write_text(harness)
            subprocess.run([
                compiler, "-std=c++17", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", "-I",
                str(root / "src/common"),
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

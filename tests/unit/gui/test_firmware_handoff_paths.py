"""Execute production handoff, picker, receipt and cleanup code on a host FS.

This is application-level fault simulation, not bootloader/FAT power-loss
certification. No printer, USB device or installed firmware is modified.
"""
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class FirmwareHandoffPathsTests(unittest.TestCase):

    def test_file_upload_invalidates_authorization_under_latch_not_in_query(
            self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        service = (root / "src/marlin_stubs/rme_file_service.cpp").read_text()
        begin = service.index('action_is(action, "WRITE_BEGIN")')
        start = service.index('            if (!claim_upload_slot', begin)
        finish = service.index('            bool matching_resume', start)
        guard = service[start:finish]
        status = service[service.index('bool report_firmware_status()'):service
                         .index('void release_upload_slot')]
        self.assertNotIn('clear_cleanup_authorization', status)
        harness = r'''
#include <cassert>
#include <array>
#include <optional>
#include <string>
bool held=false, competitor=false, clear_ok=true, firmware=true;
int invalidations=0, validation_stops=0, resume_reads=0;
std::string error;
namespace transfers { struct Monitor { enum class Outcome {ErrorStorage}; }; }
bool claim_upload_slot(const char *,unsigned) {
 if(competitor) return false;
 held=true; return true;
}
void release_upload_slot(transfers::Monitor::Outcome) { held=false; }
void report_error(const char *e) { error=e; }
bool is_firmware_candidate(const std::array<char,256>&) {return firmware;}
void stop_firmware_validation() {assert(held); ++validation_stops;}
namespace firmware_update_handoff {
bool clear_cleanup_authorization() { assert(held); ++invalidations; return clear_ok; }
}
bool begin_upload() {
 std::optional<std::array<char,256>> path=std::array<char,256>{};
 std::optional<unsigned> size=100;
''' + guard + r'''
 ++resume_reads; return true;
}
int main() {
 // Line, bulk and binary transports share this actual dispatch block.
 for(int transport=0;transport<3;++transport) {
  held=false; competitor=true; error.clear();
  int before=invalidations, reads=resume_reads;
  begin_upload();
  assert(!held && error=="transfer_busy" && invalidations==before && resume_reads==reads);
  competitor=false; clear_ok=false; error.clear();
  begin_upload();
  assert(!held && error=="cleanup_authorization_failed" && resume_reads==reads);
  clear_ok=true; error.clear(); begin_upload();
  assert(held && error.empty() && resume_reads==reads+1);
  held=false; firmware=false; before=invalidations;
  begin_upload(); assert(held && invalidations==before);
  held=false; firmware=true;
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            cpp.write_text(harness)
            binary = pathlib.Path(directory) / "test"
            subprocess.run([
                compiler, "-std=c++17", "-fsanitize=address,undefined",
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_handoff_routes_and_missing_marker_recovery(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        m997 = (root / "src/marlin_stubs/M997.cpp").read_text()
        handoff = m997[m997.index("static void update_main_board"):m997.
                       index("/** ")]
        picker = (root / "src/gui/screen_firmware_update.cpp").read_text()
        picker = picker[picker.
                        index("constexpr const char *staged_firmware_path"
                              ):picker.index("} // namespace")]
        server = (root / "src/common/marlin_server.cpp").read_text()
        start = server.index(
            "    const bool media = usb_host::is_media_inserted();")
        cleanup = server[start:server.index(
            "    if (marlin_vars().media_inserted != media)", start)]
        receipt = (root /
                   "src/common/firmware_cleanup_receipt.cpp").read_text()
        legacy = (root / "src/marlin_stubs/M998.cpp").read_text()
        legacy = legacy[legacy.index("namespace {") +
                        len("namespace {"):legacy.index("} // namespace")]
        service = (root / "src/marlin_stubs/rme_file_service.cpp").read_text()
        paths = service[service.index("bool decode_path("):service.
                        index("bool parse_sha256(")]
        flash_start = service.index(
            '    } else if (action_is(action, "PRINT") || action_is(action, "FLASH")) {'
        )
        flash_start = service.index('\n', flash_start) + 1
        flash_body = service[flash_start:service.index(
            '\n    } else {\n        return false;', flash_start)]
        preamble = r'''
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <optional>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include "firmware_cleanup_receipt.hpp"
#include "firmware_cleanup_gate.hpp"
#include "firmware_update_handoff.hpp"
#include "rme_protocol_parser.hpp"
#if __has_include("rme_firmware_compatibility.hpp")
#include "rme_firmware_compatibility.hpp"
#define PRINTER_TYPE 7
#define PRINTER_VERSION 10
#define PRINTER_SUBVERSION 0
#endif
static std::string base, fault;
static bool busy=false, armed=false, inserted=true;
static unsigned now_ms=0;
static int resets=0;
static std::string selected;
static std::string queued, error;
static bool idle=true, queue_full=false;
static int sync_count=0;
std::string mapped(const char *p) { return base + std::string(p).substr(4); }
bool fail(const std::string &key) {
    if (fault != key) return false;
    fault.clear(); errno=EIO; return true;
}
FILE *open_usb(const char *p,const char *m) {
    if (fail(std::string("open:")+p)) return nullptr;
    return fopen(mapped(p).c_str(),m);
}
int remove_usb(const char *p) {
    if (fail(std::string("remove:")+p)) return -1;
    return remove(mapped(p).c_str());
}
int rename_usb(const char *a,const char *b) {
    if (fail(std::string("rename:")+b)) return -1;
    return rename(mapped(a).c_str(),mapped(b).c_str());
}
int stat_usb(const char *p, struct stat *s) { return stat(mapped(p).c_str(),s); }
int sync_usb(int fd) {
 ++sync_count;
 if(fail("sync") || (sync_count==2 && fail("receipt-sync"))) return -1;
 return fsync(fd);
}
size_t write_usb(const void *p,size_t size,size_t count,FILE *f) {
 if(fail("write")) return 0;
 return fwrite(p,size,count,f);
}
int close_usb(FILE *f) {
 long position=ftell(f);
 int result=fclose(f);
 return position==0 && fail("close") ? -1 : result;
}
namespace transfers {
struct Monitor {
 enum class Type { Link };
 enum class Outcome { Finished, ErrorStorage, Stopped };
 struct Slot {
   bool owns=true;
   Slot() { busy=true; }
   Slot(Slot&& other) { owns=other.owns; other.owns=false; }
   ~Slot() { if(owns) busy=false; }
   void done(Outcome) {}
 };
 std::optional<int> id() { return busy ? std::optional<int>(1) : std::nullopt; }
 std::optional<Slot> allocate(Type,const char*,int) {
   if(busy) return std::nullopt;
   return std::optional<Slot>(std::in_place);
 }
 static Monitor instance;
}; Monitor Monitor::instance;
}
namespace usb_host { bool is_media_inserted() { return inserted; } }
unsigned ticks_ms() { return now_ms; }
namespace data_exchange {
 void set_reflash_bbf_sfn(const char *p) { armed=true; selected=p; }
 void clear_reflash_bbf_sfn() { armed=false; selected.clear(); }
 bool is_reflash_bbf_sfn(const char*) { return armed; }
 void fw_update_older_on_restart_enable() {}
 void fw_update_on_restart_enable() {}
}
namespace serial_remote_control {
 bool session_active() { return true; }
 enum class TransferKind { none, firmware };
 void set_transfer(TransferKind, unsigned=0, unsigned=0) {}
}
namespace SerialPrinting { void notify_workflow(const char*,const char*,const char*,int) {} }
void poll();
struct Queue {
 void ok_to_send() { assert(busy); poll(); }
 bool enqueue_one(const char *s,bool) {
  if(queue_full) return false;
  queued=s; return true;
 }
} queue;
namespace marlin_server { bool printer_idle() { return idle; } }
void report_error(const char *s) { error=s; }
bool action_is(std::string_view a,std::string_view b) { return rme_protocol::action_is(a,b); }
constexpr size_t MAX_CMD_SIZE=256, rme_path_buffer_size=256;
auto value(std::string_view a,std::string_view b) { return rme_protocol::value(a,b); }
void HAL_Delay(int) { assert(busy); poll(); }
struct Reset {};
void sys_reset() { ++resets; throw Reset{}; }
size_t strlcpy(char *d,const char *s,size_t n) {
 size_t len=strlen(s); if(n) { strncpy(d,s,n-1); d[n-1]=0; } return len;
}
#define SERIAL_ERROR_MSG(...) ((void)0)
#define SERIAL_ERROR_START() ((void)0)
#define SERIAL_ECHOLNPAIR(...) ((void)0)
#define SERIAL_ECHOLN(...) ((void)0)
#define SERIAL_ECHOLNPGM(...) ((void)0)
#define SERIAL_FLUSHTX() ((void)0)
#define HAS_USB_DEVICE() 0
#define fopen open_usb
#define remove remove_usb
#define rename rename_usb
#define fsync sync_usb
#define fwrite write_usb
#define fclose close_usb
#define stat(p,s) stat_usb(p,s)
'''
        tests = r'''
void put(const char *p,const char *v="image") {
 FILE *f=fopen(p,"wb"); assert(f);
 if(std::string(v)=="image") {
  std::array<unsigned char,115> header{};
  header[100]=6; header[101]=9; header[102]=2;
  header[111]=7; header[112]=10;
  fwrite(header.data(),1,header.size(),f);
 } else fwrite(v,1,strlen(v),f);
 fclose(f);
}
bool exists(const char *p) { return std::filesystem::exists(mapped(p)); }
void fresh() {
 inserted=false; poll(); inserted=true; now_ms+=40000;
 std::filesystem::remove_all(base); std::filesystem::create_directory(base);
 armed=false; busy=false; selected.clear(); fault.clear();
 queued.clear(); error.clear(); idle=true; queue_full=false;
 sync_count=0;
}
bool flash(const char *name) {
 int before=resets;
 try { M997_no_parser(0,0,false,name); } catch(const Reset&) {}
 return resets!=before;
}
int main(int argc,char **argv) {
 assert(argc==2); base=argv[1];
 // Plugin staged image and LCD picker use the same explicit M997 handoff.
 // Legacy M998's completed FWUPD.BBF uses the rename compatibility route.
 for (const char *route : {"plugin", "picker", "legacy", "direct"}) {
  for (const char *lost : {"none", "primary", "receipt"}) {
   fresh(); put("/usb/ORIGINAL.BBF");
   if(std::string(route)=="picker") assert(stage_firmware_for_bootloader("/usb/ORIGINAL.BBF"));
   else if(std::string(route)=="legacy") put("/usb/FWUPD.BBF");
   else put("/usb/FWUPD.RME");
   poll(); // Staging alone must not authorize cleanup.
   assert(exists(std::string(route)=="legacy" ? "/usb/FWUPD.BBF" : "/usb/FWUPD.RME"));
   const char *name=std::string(route)=="legacy" ? "FWUPD.BBF" : "FWUPD.RME";
   if(std::string(route)=="plugin") {
    file_flash("FLASH path=FWUPD.BBF");
    assert(queued=="M997 /usb/FWUPD.RME" && error.empty());
    name=queued.c_str()+10;
   }
   assert(flash(name)); assert(armed && selected=="FWUPD.RME");
   assert(exists("/usb/FWUPD.RME"));
   assert(exists("/usb/FWUPD.UI") && exists("/usb/FWUPD.CLN"));
   if(std::string(lost)=="primary") remove("/usb/FWUPD.UI");
   if(std::string(lost)=="receipt") remove("/usb/FWUPD.CLN");
   armed=false; now_ms+=40000; poll();
   assert(!exists("/usb/FWUPD.RME") && !exists("/usb/FWUPD.CLN") && !exists("/usb/FWUPD.UI"));
   assert(exists("/usb/ORIGINAL.BBF"));
  }
 }
 // Invalid receipts never authorize deletion.
 for(const char *bad : {"", "RME", "RME flash handoff v1\nextra"}) {
  fresh(); put("/usb/FWUPD.RME"); put("/usb/FWUPD.CLN",bad); poll();
  assert(exists("/usb/FWUPD.RME"));
 }
 fresh(); put("/usb/FWUPD.RME"); assert(flash("FWUPD.RME"));
 remove("/usb/FWUPD.UI"); remove("/usb/FWUPD.CLN");
 armed=false; now_ms+=40000; poll();
 assert(exists("/usb/FWUPD.RME")); // No proof of handoff: fail safely.
 // Failed persistence cannot reset or authorize a later unrequested flash.
 for(const char *failure : {"open:/usb/FWUPD.UI.tmp", "open:/usb/FWUPD.TCL", "sync", "receipt-sync", "write", "close", "rename:/usb/FWUPD.CLN"}) {
  fresh(); put("/usb/FWUPD.RME"); fault=failure;
  assert(!flash("FWUPD.RME")); assert(!armed);
  assert(!exists("/usb/FWUPD.UI") && !exists("/usb/FWUPD.CLN"));
  now_ms+=40000; poll(); assert(exists("/usb/FWUPD.RME"));
 }
 fresh(); put("/usb/FWUPD.RME"); fault="rename:/usb/FWUPD.UI";
 assert(flash("FWUPD.RME")); assert(exists("/usb/FWUPD.CLN"));
 armed=false; now_ms+=40000; poll(); assert(!exists("/usb/FWUPD.RME"));
 // Picker replacement invalidates previous flash authorization and metadata.
 fresh(); put("/usb/FWUPD.UI"); put("/usb/FWUPD.CLN"); put("/usb/ORIGINAL.BBF");
 put("/usb/FWUPD.RME.rme-verified");
 assert(stage_firmware_for_bootloader("/usb/ORIGINAL.BBF")); poll();
 assert(exists("/usb/FWUPD.RME") && !exists("/usb/FWUPD.UI") && !exists("/usb/FWUPD.CLN"));
 assert(!exists("/usb/FWUPD.RME.rme-verified"));
 // Arbitrary direct firmware selection must not consume the original image.
 fresh(); put("/usb/ORIGINAL.BBF"); assert(flash("ORIGINAL.BBF"));
 armed=false; now_ms+=40000; poll(); assert(exists("/usb/ORIGINAL.BBF"));
 fresh(); put("/usb/FWUPD.RME"); busy=true;
 assert(!flash("FWUPD.RME")); busy=false;
 // Exercise the actual legacy uploader, including its SHA and base64 code.
 fresh(); put("/usb/FWUPD.UI"); put("/usb/FWUPD.CLN");
 const char *begin="S5 H6105d6cc76af400325e94d588ce511be5bfdbb73b437dc51eca43917d7a43e3d";
 legacy::begin_upload(begin);
 assert(legacy::upload.active && busy);
 assert(!exists("/usb/FWUPD.UI") && !exists("/usb/FWUPD.CLN"));
 assert(!flash("FWUPD.BBF")); // Cannot flash a partial upload.
 legacy::write_chunk("O0 DaW1hZ2U="); legacy::finish_upload();
 assert(!legacy::upload.active && !busy && exists("/usb/FWUPD.BBF"));
 poll(); assert(exists("/usb/FWUPD.BBF"));
 put("/usb/FWUPD.BBF"); // Handoff validation uses a structurally valid BBF header.
 assert(flash("FWUPD.BBF")); armed=false; now_ms+=40000; poll();
 assert(!exists("/usb/FWUPD.RME") && !exists("/usb/FWUPD.BBF"));
 // Every cleanup failure retains durable authorization for another poll.
 for(const char *path : {"/usb/FWUPD.RME", "/usb/FWUPD.BBF", "/usb/FWUPD.UI", "/usb/FWUPD.CLN", "/usb/FWUPD.TCL"}) {
  fresh(); put("/usb/FWUPD.RME"); assert(flash("FWUPD.RME"));
  put("/usb/FWUPD.BBF"); put("/usb/FWUPD.TCL"); armed=false;
  fault=std::string("remove:")+path; now_ms+=40000; poll();
  assert(exists("/usb/FWUPD.UI") || exists("/usb/FWUPD.CLN"));
  now_ms+=1000; poll();
  assert(!exists("/usb/FWUPD.RME") && !exists("/usb/FWUPD.BBF"));
  assert(!exists("/usb/FWUPD.CLN") && !exists("/usb/FWUPD.TCL"));
 }
 fresh(); busy=true; legacy::begin_upload(begin);
 assert(!legacy::upload.active); busy=false;
 for(const char *bad : {"", "S5", "S5 H", "S5 H1", "S5 H12", "S5 Hbad"}) {
  fresh(); legacy::begin_upload(bad);
  assert(!legacy::upload.active && !busy);
 }
 fresh(); fault="remove:/usb/FWUPD.UI"; legacy::begin_upload(begin);
 assert(!legacy::upload.active && !busy && !exists("/usb/FWUPD.TMP"));
 // Real FILE FLASH dispatch must not enqueue while printing or transferring.
 fresh(); busy=true; file_flash("FLASH path=FWUPD.RME");
 assert(queued.empty() && error=="transfer_busy"); busy=false;
 error.clear(); idle=false; file_flash("FLASH path=FWUPD.RME");
 assert(queued.empty() && error=="printer_busy"); idle=true;
 error.clear(); queue_full=true; file_flash("FLASH path=FWUPD.RME");
 assert(queued.empty() && error=="queue_full"); queue_full=false;
 error.clear(); file_flash("FLASH path=NOTIMAGE.TXT");
 assert(queued.empty() && error=="invalid_firmware");
}
'''
        harness = (preamble + receipt + handoff + picker +
                   "\nnamespace legacy {\n" + legacy + "\n}\n" + paths + '''
void file_flash(std::string_view action) {
 auto path=path_value(action); assert(path);
''' + flash_body + "\n}\nvoid poll() {\n" + cleanup + "\n}\n" + tests)
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            cpp.write_text(harness)
            binary = pathlib.Path(directory) / "test"
            mbedtls = root / "lib/Middlewares/Third_Party/mbedtls"
            objects = []
            for name in ("sha256", "base64", "platform_util", "constant_time"):
                obj = pathlib.Path(directory) / (name + ".o")
                subprocess.run([
                    shutil.which("cc"), "-ffunction-sections",
                    "-fdata-sections", "-fsanitize=address,undefined", "-I",
                    str(mbedtls / "include"), "-c",
                    str(mbedtls / "library" / (name + ".c")), "-o",
                    str(obj)
                ],
                               check=True)
                objects.append(str(obj))
            subprocess.run([
                compiler, "-std=c++20", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", "-I",
                str(root / "src/common"), "-I",
                str(mbedtls / "include"),
                str(cpp), *objects, "-Wl,--gc-sections", "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary),
                            str(pathlib.Path(directory) / "usb")],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

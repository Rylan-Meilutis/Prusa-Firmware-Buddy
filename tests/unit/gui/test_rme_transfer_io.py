"""Execute production storage wait, download branch and UI lease code."""
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]


class TransferIoTests(unittest.TestCase):

    def test_compact_ready_records_preserve_all_wire_formats(self):
        source = (ROOT / "src/marlin_stubs/rme_file_service.cpp").read_text()
        start = source.index("[[gnu::noinline]] void report_upload_ready(")
        ready = source[start:source.index("struct ReadChunkResult {", start)]
        harness = r'''
#include <cassert>
#include <cstdint>
#include <string>
std::string wire;
struct {bool binary=false,bulk=false; unsigned received=0;} upload;
constexpr unsigned binary_chunk_size=512,bulk_chunk_size=384,transfer_chunk_size=48;
constexpr uint8_t binary_window_size=3,bulk_window_size=4;
void emit(const char *text){wire+=text;}
template<class T> void emit(T value){wire+=std::to_string(value);}
#define SERIAL_ECHOPGM(x) emit(x)
#define SERIAL_ECHO(x) emit(x)
#define SERIAL_EOL() emit("\n")
''' + ready + r'''
int main() {
    for(bool resumed:{false,true}) for(unsigned mode=0;mode<3;++mode) {
        upload.binary=mode==2; upload.bulk=mode==1; upload.received=resumed?123:0;
        wire.clear(); report_upload_ready(resumed);
        std::string expected="RME_FILE_";
        expected+=mode==2?"BINARY":mode==1?"BULK":"WRITE";
        expected+="_READY offset="+std::to_string(upload.received);
        expected+=" chunk="+std::to_string(mode==2?512:mode==1?384:48);
        if(mode) expected+=" window="+std::to_string(mode==2?3:4);
        if(mode==2) expected+=" header=10 endian=little crc=crc32";
        if(resumed) expected+=" resumed=1";
        expected+="\n"; assert(wire==expected);
    }
}
'''
        self.compile_run(harness)

    def compile_run(self, source):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            code = path / "test.cpp"
            code.write_text(source)
            result = subprocess.run([
                compiler, "-std=c++20", "-pthread", "-Wall", "-Wextra",
                "-Werror", "-I",
                str(ROOT / "src/common"), "-I",
                str(ROOT / "lib/SG14"),
                str(code), "-o",
                str(path / "test")
            ],
                                    capture_output=True,
                                    text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(path / "test"), str(path)],
                           check=True,
                           timeout=20,
                           capture_output=True)

    def test_real_worker_wait_preserves_buffers_errno_and_thermal_service(
            self):
        source = (ROOT / "src/marlin_stubs/rme_file_service.cpp").read_text()
        start = source.index("intptr_t rme_file_io::run(")
        end = source.index("\nnamespace {", start)
        production = source[start:end]
        self.assertNotIn("::idle(", production)
        self.assertNotIn("watchdog_refresh(", production)
        harness = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#define SG14_INPLACE_FUNCTION_THROW(x) std::abort()
#include "rme_file_io.hpp"
struct AsyncJobExecutionControl {};
bool cleanup_ok=true;
namespace firmware_update_handoff {bool clear_cleanup_authorization(){return cleanup_ok;}}
bool make_dirs(const char *){return true;}
const auto marlin_thread = std::this_thread::get_id();
class AsyncJob {
    std::thread worker;
    std::atomic<bool> active{false};
public:
    template<class F> void issue(F operation) {
        static unsigned requests=0;
        const unsigned delay=++requests==1 ? 4200 : 1;
        active = true;
        worker = std::thread([this, operation, delay]{
            assert(std::this_thread::get_id()!=marlin_thread);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            AsyncJobExecutionControl control; operation(control);
            active.store(false, std::memory_order_release);
        });
    }
    bool is_active() const { return active.load(std::memory_order_acquire); }
    ~AsyncJob() { if (worker.joinable()) worker.join(); }
};
struct { unsigned checks=0; void manage_heater(){++checks; errno=EBUSY;} } thermalManager;
void osDelay(unsigned ms){ std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
''' + production + r'''
int main(int argc, char **argv) {
    assert(argc==2);
    char buffer[16]="completed";
    errno = EIO;
    const auto missing=std::string(argv[1])+"/missing.bin";
    assert(rme_file_io::fopen(missing.c_str(),"rb")==nullptr && errno==ENOENT);
    assert(thermalManager.checks>100);
    assert(rme_file_io::prepare_directory(argv[1]));
    assert(rme_file_io::clear_cleanup()); cleanup_ok=false;
    assert(!rme_file_io::clear_cleanup()); cleanup_ok=true;
    const std::string file=std::string(argv[1])+"/upload.bin";
    FILE *f=rme_file_io::fopen(file.c_str(),"wb"); assert(f);
    assert(rme_file_io::fwrite(buffer,9,f)==9);
    assert(rme_file_io::fflush(f)==0);
    assert(rme_file_io::flush_sync(f)==0);
    assert(rme_file_io::fclose(f)==0);
    struct stat st{}; assert(rme_file_io::stat(file.c_str(),&st)==0 && st.st_size==9);
    f=rme_file_io::fopen(file.c_str(),"rb"); assert(f);
    assert(rme_file_io::fseek(f,0,SEEK_SET)==0);
    char read[16]{}; assert(rme_file_io::fread(read,9,f)==9);
    assert(strcmp(buffer,read)==0 && rme_file_io::fclose(f)==0);
    const auto temporary=file+".meta.tmp", metadata=file+".meta";
    assert(rme_file_io::write_metadata(temporary.c_str(),metadata.c_str(),buffer,9));
    char chunk_data[16]{};
    rme_file_io::ReadChunk chunk{metadata.c_str(),2,chunk_data,4};
    assert(rme_file_io::read_chunk(chunk) && chunk.file_size==9 && chunk.count==4);
    assert(strncmp(chunk_data,"mple",4)==0);
    const auto invalid=file+"/missing/meta";
    assert(!rme_file_io::write_metadata(temporary.c_str(),invalid.c_str(),buffer,9));
    assert(::stat(temporary.c_str(),&st)!=0);
    const auto renamed=file+".renamed";
    assert(rme_file_io::rename(file.c_str(),renamed.c_str())==0);
    assert(rme_file_io::remove(renamed.c_str())==0);
    assert(rme_file_io::fopen(file.c_str(),"rb")==nullptr && errno==ENOENT);
}
'''
        self.compile_run(harness)

    def test_text_download_real_byte_progress_exact_eof_and_errors(self):
        source = (ROOT / "src/marlin_stubs/rme_file_service.cpp").read_text()
        start = source.index('    } else if (action_is(action, "READ")) {')
        end = source.index('    } else if (action_is(action, "WRITE_BEGIN")',
                           start)
        branch = source[source.index("\n", start) + 1:end]
        helper = source[source.index("struct ReadChunkResult {"):source.
                        index("void report_upload_error(")]
        worker_start = source.index("intptr_t rme_file_io::run(")
        worker = source[worker_start:source.index("\nnamespace {", worker_start
                                                  )]
        harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cerrno>
#include "rme_file_io.hpp"
namespace file_io=rme_file_io;
struct AsyncJobExecutionControl{};
namespace firmware_update_handoff {bool clear_cleanup_authorization(){return true;}}
bool make_dirs(const char *){return true;}
struct AsyncJob {
    template<class F> void issue(F operation){AsyncJobExecutionControl control; operation(control);}
    bool is_active() const {return false;}
};
struct {void manage_heater(){}} thermalManager;
void osDelay(unsigned){}
''' + worker + r'''
namespace transfers { struct Monitor {
    static Monitor instance; bool busy=false;
    std::optional<int> id() { return busy ? std::optional<int>(1) : std::nullopt; }
}; Monitor Monitor::instance; }
namespace serial_remote_control {
enum class TransferKind{none,file};
struct Update { TransferKind kind; unsigned done,total,lease; };
std::vector<Update> updates;
void set_transfer(TransferKind kind, unsigned done=0, unsigned total=0, unsigned lease=0) {
    updates.push_back({kind,done,total,lease});
}
}
std::string error;
void report_error(const char *code){error=code;}
unsigned offset_arg=0, length_arg=48;
std::optional<unsigned> number(int,const char *key) {
    return std::string(key)=="offset" ? offset_arg : length_arg;
}
int mbedtls_base64_encode(unsigned char *out,size_t,size_t *encoded,const unsigned char *,size_t count) {
    out[0]='X'; out[1]=0; *encoded=count ? 1 : 0; return 0;
}
#define SERIAL_ECHOPGM(x) (void)(x)
#define SERIAL_ECHO(x) (void)(x)
#define SERIAL_ECHOLN(x) (void)(x)
constexpr unsigned transfer_chunk_size=48, maximum_file_size=1024U*1024*1024;
constexpr unsigned download_progress_lease_ms=15000;
std::array<uint8_t,512> binary_payload{};
std::optional<std::array<char,256>> path;
''' + helper + r'''
bool read_chunk() { int command=0;
''' + branch + r'''
    return true;
}
int main(int argc,char **argv) {
    assert(argc==2); path.emplace();
    const auto name=std::string(argv[1])+"/part.bin";
    strcpy(path->data(),name.c_str());
    FILE *f=fopen(name.c_str(),"wb"); assert(f);
    std::array<char,96> data{}; assert(fwrite(data.data(),1,data.size(),f)==96); fclose(f);
    using namespace serial_remote_control;
    assert(read_chunk() && error.empty());
    assert(updates.back().kind==TransferKind::file && updates.back().done==48 && updates.back().total==96);
    assert(updates.back().lease==15000);
    offset_arg=48; assert(read_chunk() && error.empty());
    assert(updates[updates.size()-2].done==96 && updates.back().kind==TransferKind::none);
    length_arg=49; read_chunk(); assert(error=="invalid_range");
    error.clear(); length_arg=48; offset_arg=maximum_file_size+1;
    read_chunk(); assert(error=="invalid_range");
    offset_arg=0; error.clear();
    transfers::Monitor::instance.busy=true; read_chunk(); assert(error=="transfer_busy");
    transfers::Monitor::instance.busy=false;
    error.clear(); remove(name.c_str()); read_chunk();
    assert(error=="read_failed" && updates.back().kind==TransferKind::none);
    error.clear(); f=fopen(name.c_str(),"wb"); fclose(f); read_chunk();
    assert(error.empty() && updates.back().kind==TransferKind::none);
}
'''
        self.compile_run(harness)

    def test_transfer_lease_expires_but_upload_is_not_timed_out(self):
        source = (ROOT / "src/gui/serial_remote_control.cpp").read_text()
        start = source.index("void set_transfer(")
        end = source.index("\nnamespace {", start)
        harness = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
uint32_t now_ms=0;
uint32_t ticks_ms(){return now_ms;}
int32_t ticks_diff(uint32_t a,uint32_t b){return int32_t(a-b);}
enum class TransferKind:uint8_t {none,file,firmware};
struct TransferStatus {TransferKind kind; uint8_t progress;};
std::atomic<TransferKind> remote_transfer_kind{TransferKind::none};
std::atomic<uint8_t> remote_transfer_progress{0};
std::atomic<uint32_t> remote_transfer_expires_ms{0};
void set_transfer(TransferKind,uint32_t=0,uint32_t=0,uint32_t=0);
''' + source[start:end] + r'''
int main() {
    set_transfer(TransferKind::file,48,96,15000);
    assert(transfer_status().progress==50);
    now_ms=14000; assert(transfer_status().kind==TransferKind::file);
    set_transfer(TransferKind::file,72,96,15000);
    now_ms=28000; assert(transfer_status().progress==75);
    now_ms=29000; assert(transfer_status().kind==TransferKind::none);
    set_transfer(TransferKind::file,1,4);
    now_ms+=100000; assert(transfer_status().progress==25);
    set_transfer(TransferKind::none); assert(transfer_status().kind==TransferKind::none);
    now_ms=UINT32_MAX-1000; set_transfer(TransferKind::file,1,2,15000);
    now_ms=13000; assert(transfer_status().kind==TransferKind::file);
    now_ms=14000; assert(transfer_status().kind==TransferKind::none);
}
'''
        self.compile_run(harness)

    def test_upload_metadata_and_commit_io_use_worker_dispatch(self):
        source = (ROOT / "src/marlin_stubs/rme_file_service.cpp").read_text()
        source = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)
        self.assertFalse(
            re.search(
                r"(?<![:\w])(fopen|fclose|fread|fwrite|fseek|fflush|fsync|stat|remove|rename|mkdir)\(",
                source))

    def test_compact_language_table_matches_original_lookup(self):
        source = (ROOT / "src/lang/translator.cpp").read_text()
        methods = source[source.index("bool Translations::RegisterProvider("):]
        harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
struct ITranslationProvider {unsigned id;};
struct Record {
    const ITranslationProvider *provider=nullptr;
    uint16_t langCode=0;
    bool operator==(const Record &) const = default;
};
struct Translations {
    using TranRec=Record;
    std::array<Record,4> translations{};
    const ITranslationProvider *currentProvider=nullptr;
    bool RegisterProvider(uint16_t,const ITranslationProvider *);
    bool ChangeLanguage(uint16_t);
    bool LangExists(uint16_t) const;
};
struct Reference: Translations {
    bool RegisterProvider(uint16_t code,const ITranslationProvider *provider) {
        auto i=std::find_if(translations.begin(),translations.end(),[&](auto&r){return r.langCode==code;});
        if(i==translations.end()) {
            i=std::find_if(translations.begin(),translations.end(),[](auto&r){return r.langCode==0;});
            if(i==translations.end()) return false;
            i->langCode=code;
        }
        i->provider=provider; return true;
    }
    bool ChangeLanguage(uint16_t code) {
        if(!code)return false;
        auto i=std::find_if(translations.begin(),translations.end(),[&](auto&r){return r.langCode==code;});
        if(i==translations.end())return false;
        currentProvider=i->provider; return true;
    }
    bool LangExists(uint16_t code) const {
        return std::any_of(translations.begin(),translations.end(),[&](auto&r){return r.langCode==code;});
    }
};
''' + methods + r'''
int main() {
    const std::array<ITranslationProvider,3> providers{{{1},{2},{3}}};
    for(unsigned seed=1;seed<50;++seed) {
        Translations actual; Reference expected; uint32_t random=seed;
        for(unsigned n=0;n<2000;++n) {
            random=random*1664525+1013904223;
            uint16_t code=(random>>8)%7;
            const auto *provider=&providers[(random>>16)%3];
            if(random&1) assert(actual.RegisterProvider(code,provider)==expected.RegisterProvider(code,provider));
            else assert(actual.ChangeLanguage(code)==expected.ChangeLanguage(code));
            for(uint16_t probe=0;probe<8;++probe)
                assert(actual.LangExists(probe)==expected.LangExists(probe));
            assert(actual.translations==expected.translations);
            assert(actual.currentProvider==expected.currentProvider);
        }
    }
}
'''
        self.compile_run(harness)


if __name__ == "__main__":
    unittest.main()

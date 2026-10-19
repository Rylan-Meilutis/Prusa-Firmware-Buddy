"""Execute actual glyph loading and cell rendering with failed resource reads."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required")
class FontRecovery(unittest.TestCase):

    def compile_run(self, directory, code, args=()):
        path = Path(directory)
        (path / "test.cpp").write_text(code)
        binary = path / "test"
        subprocess.run([
            "c++", "-std=c++20", "-fsanitize=address,undefined", "-g", "-I",
            str(ROOT / "src/gui"), "-I",
            str(ROOT / "src/common"), "-I",
            str(ROOT / "src/common/utils"), "-I",
            str(ROOT / "include"), "-I",
            str(ROOT / "tests/unit/lang/translator"),
            str(path / "test.cpp"), "-o",
            str(binary)
        ],
                       check=True)
        subprocess.run([str(binary), *args],
                       check=True,
                       env=dict(os.environ,
                                ASAN_OPTIONS="detect_leaks=0",
                                UBSAN_OPTIONS="halt_on_error=1"))

    def test_builtin_ascii_and_numbers_ignore_missing_resources_and_cache_recovers(
            self):
        source = (ROOT / "src/gui/fonts.cpp").read_text()
        loader = "bool load_external_font_glyph(" + source.split(
            "bool load_external_font_glyph(", 1)[1].split("\n#endif", 1)[0]
        code = r'''
#include <font_builtin_ascii.hpp>
#include <array>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <string>
struct font_t { uint8_t w,h; FontCharacterSet charset; const void *pcs; };
std::string directory;
int opens=0, reads=0, fail_at=-1;
int font_open(const char *path,int mode) {
 ++opens;
 return ::open((directory+"/"+std::string(path).substr(std::string(path).find_last_of('/')+1)).c_str(),mode);
}
ssize_t font_read(int fd,void *out,size_t size) {
 if(reads++==fail_at) return -1;
 return ::read(fd,out,size);
}
#define BOARD_IS_XBUDDY() 1
#define open font_open
#define read font_read
'''
        main = r'''
#undef open
#undef read
int main(int argc,char **argv) {
 assert(argc==2); directory=argv[1];
 using namespace buddy::builtin_font;
 const font_t fonts[]={{9,16,FontCharacterSet::full,nullptr},{11,19,FontCharacterSet::full,nullptr},
   {13,22,FontCharacterSet::full,nullptr},{30,53,FontCharacterSet::digits,nullptr}};
 const uint8_t *original[]={detail::font_regular_9x16_data,detail::font_bold_11x19_data,
   detail::font_bold_13x22_data,detail::font_bold_30x53_data};
 std::array<uint8_t,797> output;
 for(int f=0;f<4;++f) {
   const auto &font=fonts[f]; size_t size=((font.w+1)/2)*font.h;
   for(uint32_t glyph=0;glyph<(f==3?16:95);++glyph) {
     output.fill(0xa5);
     assert(load_external_font_glyph(&font,glyph,output.data()+1,size));
     assert(output.front()==0xa5 && output[size+1]==0xa5);
     assert(memcmp(output.data()+1,original[f]+glyph*size,size)==0);
   }
 }
 assert(opens==0 && reads==0); // even with /internal absent, no filesystem dependency
 assert(!load_external_font_glyph(nullptr,0,output.data(),72));
 assert(!load_external_font_glyph(&fonts[0],0,nullptr,72));
 assert(!load_external_font_glyph(&fonts[0],0,output.data(),796));
 std::array<uint8_t,80> before,after;
 assert(load_external_font_glyph(&fonts[0],95,before.data(),80));
 const int initial_reads=reads;
 fail_at=reads+6; // partial decode of a DIFFERENT extended glyph
 assert(!load_external_font_glyph(&fonts[0],96,after.data(),80));
 fail_at=-1;
 const int failed_reads=reads;
 assert(load_external_font_glyph(&fonts[0],95,after.data(),80));
 assert(reads>failed_reads && failed_reads>initial_reads);
 assert(before==after); // failure must not poison the previous glyph identity
}
'''
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run([
                sys.executable,
                str(ROOT / "utils/resources/font_header_to_bin.py"),
                str(ROOT / "src/gui/res/cc/font_regular_9x16_full.hpp"),
                str(Path(directory) / "9x16.bin")
            ],
                           check=True)
            self.compile_run(directory, code + loader + main, (directory, ))

    def test_actual_renderer_never_sends_old_pixels_after_a_failed_glyph(self):
        source = (ROOT / "src/guiapi/src/display_ex.cpp").read_text()
        render = "void store_char_in_buffer(" + source.split(
            "void store_char_in_buffer(", 1)[1].split(
                "/// Draws a rectangle boundary", 1)[0]
        code = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#define PRINTER_IS_PRUSA_MINI() 0
#define BOARD_IS_XBUDDY() 1
using unichar=uint32_t;
struct font_t { uint8_t w,h,bpr; };
constexpr size_t BuffAlphaLen=16;
struct StoreCharInBufferMeasure { explicit StoreCharInBufferMeasure(const font_t *) {} };
struct Color { uint8_t value; };
constexpr size_t STORE_FN_PIXEL_SIZE=3;
std::array<uint8_t,5760> pixels;
bool resource_ok;
struct DispBuffer {
 Color bg,fg;
 DispBuffer(int,Color bg,Color fg):bg(bg),fg(fg) {}
 void OffsetInsert(uint8_t alpha,uint32_t offset) {
   assert(offset+3<=pixels.size());
   for(int i=0;i<3;++i) pixels[offset+i]=alpha ? fg.value : bg.value;
 }
};
uint32_t get_char_position_in_font(unichar c,const font_t *) { return c; }
bool load_external_font_glyph(const font_t *,uint32_t,uint8_t *out,size_t size) {
 if(resource_ok) memset(out,0xff,size);
 return resource_ok;
}
'''
        main = r'''
int main() {
 for(const font_t &font : {font_t{9,16,5},font_t{11,19,6},font_t{13,22,7},font_t{30,53,15}}) {
   pixels.fill(0xa5); resource_ok=false;
   store_char_in_buffer(1,0,'A',&font,{0},{255});
   const size_t used=font.w*font.h*3;
   for(size_t i=0;i<used;++i) assert(pixels[i]==0);
   for(size_t i=used;i<pixels.size();++i) assert(pixels[i]==0xa5);
   pixels.fill(0xa5); resource_ok=true;
   store_char_in_buffer(1,0,'A',&font,{0},{255});
   for(size_t i=0;i<used;++i) assert(pixels[i]==255);
 }
 const font_t font{9,16,5}; pixels.fill(0xa5); resource_ok=true;
 store_char_in_buffer(2,0,'A',&font,{0},{255});
 resource_ok=false;
 store_char_in_buffer(2,1,'B',&font,{0},{255});
 for(size_t y=0;y<16;++y) for(size_t x=0;x<18;++x)
   for(size_t byte=0;byte<3;++byte) assert(pixels[(y*18+x)*3+byte]==(x<9?255:0));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            self.compile_run(directory, code + render + main)


if __name__ == "__main__":
    unittest.main()

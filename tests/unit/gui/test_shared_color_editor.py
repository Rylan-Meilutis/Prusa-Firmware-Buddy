"""Exercise the size-shared color dialog and complete theme equality."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class SharedColorEditorTests(unittest.TestCase):

    def test_production_color_editor(self):
        source = (ROOT / "src/gui/screen_menu_led_colors.cpp").read_text()
        structure = source[source.index("struct ThemeColors {"):source.
                           index("constexpr ThemeColors theme_prusa")]
        functions = source[source.index("bool same_theme("):source.index(
            "#if HAS_LEDS()", source.index("bool same_theme("))]
        hex_parser = source[source.index("bool parse_hex_color_token("):source.
                            index("bool token_equals(")]
        program = r'''
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
using string_view_utf8 = const char *;
struct StringBuilder {
 std::array<char,8> &buffer;
 StringBuilder(std::array<char,8> &b):buffer(b) {}
 void append_printf(const char *fmt, unsigned long value) { std::snprintf(buffer.data(),buffer.size(),fmt,value); }
};
struct DialogTextInput {
 static inline bool accept=true;
 static inline const char *input="123abc";
 static inline char initial[8];
 static bool exec(const string_view_utf8 &, std::array<char,8> &b) {
  std::strcpy(initial,b.data());
  std::snprintf(b.data(),b.size(),"%s",input);
  return accept;
 }
};
template<int Tag> struct Store {
 uint32_t value=0xabcdef12; int writes=0;
 uint32_t get() {return value;}
 void set(uint32_t v) {value=v; ++writes;}
};
enum { JSMN_STRING, JSMN_OBJECT };
struct jsmntok_t { int type, start, end; };
''' + structure + functions + hex_parser + r'''
int main() {
 Store<0> a; Store<1> b;
 assert(edit_color("a",a) && a.value==0x123abc && a.writes==1);
 assert(std::strcmp(DialogTextInput::initial,"cdef12")==0);
 DialogTextInput::input="#ffffff";
 assert(edit_color("b",b) && b.value==0xffffff && b.writes==1);
 DialogTextInput::input="000000";
 assert(edit_color("a",a) && a.value==0);
 for(const char *invalid : {"", "#", "xyz", "1000000"}) {
  DialogTextInput::input=invalid; auto before=a.writes;
  assert(!edit_color("a",a) && a.value==0 && a.writes==before);
 }
 DialogTextInput::accept=false; DialogTextInput::input="123456";
 auto before=a.writes;
 assert(!edit_color("a",a) && a.value==0 && a.writes==before);
 ThemeColors x{}, y{}; assert(same_theme(x,y));
 auto fields=std::array{&ThemeColors::primary,&ThemeColors::progress,&ThemeColors::warning,
  &ThemeColors::error,&ThemeColors::image,&ThemeColors::led_idle,&ThemeColors::led_printing,
  &ThemeColors::led_finished,&ThemeColors::led_warning,&ThemeColors::led_error};
 for(auto field:fields) { y.*field=1; assert(!same_theme(x,y)); y.*field=0; }
 assert(same_theme(x,y));
 uint32_t color=42;
 for(const char *valid : {"ffffff","000000","Aa09fF","#123456"}) {
  jsmntok_t token{JSMN_STRING,0,static_cast<int>(std::strlen(valid))};
  assert(parse_hex_color_token(valid,token,color));
 }
 for(const char *invalid : {"fffffg"," 12345","12345 ","+12345","12345","1234567","#12345g","0x1234"}) {
  jsmntok_t token{JSMN_STRING,0,static_cast<int>(std::strlen(invalid))};
  color=42; assert(!parse_hex_color_token(invalid,token,color) && color==42);
 }
 assert(!parse_hex_color_token("ffffff",{JSMN_OBJECT,0,6},color));
 assert(!parse_hex_color_token("ffffff",{JSMN_STRING,-1,6},color));
 assert(!parse_hex_color_token("ffffff",{JSMN_STRING,5,1},color));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "colors"
            subprocess.run([
                "c++", "-std=c++20", "-x", "c++", "-fsanitize=undefined", "-o",
                str(executable), "-"
            ],
                           input=program,
                           text=True,
                           check=True)
            subprocess.run([str(executable)], check=True)

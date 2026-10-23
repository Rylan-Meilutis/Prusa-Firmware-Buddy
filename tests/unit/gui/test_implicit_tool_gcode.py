"""Execute production parked-tool handlers; never send commands to hardware."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from test_serial_pause_recovery import function

ROOT = Path(
    os.environ.get('RME_TEST_ROOT',
                   Path(__file__).resolve().parents[3]))


class ImplicitToolGcode(unittest.TestCase):

    def run_cpp(self, code):
        if not shutil.which('c++'):
            self.skipTest('C++ compiler required')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(code)
            subprocess.run([
                'c++', '-std=c++23', '-fsanitize=address,undefined',
                '-fno-pie', '-no-pie',
                str(path / 'test.cpp'), '-o',
                str(path / 'test')
            ],
                           check=True)
            subprocess.run([str(path / 'test')],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))

    def test_virtual_resolver_and_flow_with_parked_picked_invalid_and_disabled_tools(
            self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/gcode.cpp').read_text()
        if 'get_target_virtual_from_optional' not in source:
            self.skipTest('Legacy branch uses the older extruder resolver')
        flow = (ROOT /
                'lib/Marlin/Marlin/src/gcode/config/M221.cpp').read_text()
        code = r'''
#include <variant>
#include <optional>
#include <cassert>
#include <cstdint>
struct NoTool {}; struct ToolNotMapped {}; struct ToolParsingError {const char *msg;};
struct VirtualToolIndex;
using Target=std::variant<VirtualToolIndex,NoTool,ToolNotMapped,ToolParsingError>;
struct VirtualToolIndex {
 uint8_t index; static constexpr uint8_t count=3;
 static auto from_raw(uint8_t i) { return VirtualToolIndex{i}; }
 bool is_enabled() const {return index!=1;}
 auto to_raw() const {return index;}
 operator unsigned() const {return index;}
 static Target currently_selected();
};
Target selected=NoTool{};
Target VirtualToolIndex::currently_selected() {return selected;}
template<class...F> struct Overload:F... {using F::operator()...;};
template<class V,class...F> auto match(V v,F... f) {return std::visit(Overload<F...>{f...},v);}
namespace stdext {
 auto to_variant(Target v) {return v;}
 template<class T> std::optional<T> get_optional(const Target &v) {
  if (auto p=std::get_if<T>(&v)) return *p;
  return std::nullopt;
 }
}
int errors=0;
#define SERIAL_ECHO_START() ((void)0)
#define SERIAL_ECHOLNPAIR(...) (++errors)
#define SERIAL_CHAR(...) ((void)0)
#define SERIAL_ECHOPAIR(...) ((void)0)
#define SERIAL_EOL() ((void)0)
#define HAS_GCODE_COMPATIBILITY() 0
struct Parser { bool t=false,s=false; uint8_t tool=0; int value=100;
 bool seenval(char c) {return c=='T'?t:s;}
 int value_int() {return value;}
} parser;
struct Planner {int flow_percentage[3]={80,81,82}; int refreshes=0;
 void refresh_e_factor(VirtualToolIndex) {++refreshes;}
} planner;
struct GcodeSuite {
 using VirtualToolFromCommand=Target;
 static Target get_virtual_tool_from_command(uint8_t i,bool) {
   if(i<3) return VirtualToolIndex{i};
   return ToolParsingError{"Invalid tool index"};
 }
 static Target get_target_virtual_from_optional(std::optional<uint8_t>,bool);
 static auto get_target_virtual_from_command_p() {
   return get_target_virtual_from_optional(parser.t?std::optional(parser.tool):std::nullopt,true);
 }
 static void M221();
};
''' + function(
            source,
            'GcodeSuite::VirtualToolFromCommand GcodeSuite::get_target_virtual_from_optional('
        ) + '\n' + function(flow, 'void GcodeSuite::M221()') + r'''
int main() {
 parser.s=true; parser.value=100; GcodeSuite::M221();
 assert(errors==0); assert(planner.flow_percentage[0]==100 && planner.flow_percentage[2]==100);
 assert(planner.flow_percentage[1]==81); // Disabled slots unchanged
 parser.t=true; parser.tool=99; parser.value=90; GcodeSuite::M221();
 assert(errors>0 && planner.flow_percentage[0]==100);
 auto count=errors; parser.t=false; selected=VirtualToolIndex{2}; GcodeSuite::M221();
 assert(errors==count && planner.flow_percentage[0]==100 && planner.flow_percentage[2]==90);
 parser.t=true; parser.tool=0; parser.value=95; GcodeSuite::M221();
 assert(planner.flow_percentage[0]==95 && planner.flow_percentage[2]==90);
 parser.tool=1; GcodeSuite::M221(); assert(errors>count && planner.flow_percentage[1]==81);
}
'''
        self.run_cpp(code)

    def test_parked_tool_global_motion_limits_and_explicit_invalid_rejection(
            self):
        source = (
            ROOT /
            'lib/Marlin/Marlin/src/gcode/config/M200-M205.cpp').read_text()
        if 'get_target_physical_from_command' not in source:
            self.skipTest('Legacy branch uses the older extruder resolver')
        for distinct in (0, 1):
            code = r'''
#include <optional>
#include <cassert>
#include <cstdint>
#define HAS_INDX() 0
#define E_STEPPERS 2
#define E_AXIS_N(e) (3 + DISTINCT*(e))
#define LOOP_XYZ(i) for(uint8_t i=0;i<3;++i)
#define LOOP_XYZE(i) for(uint8_t i=0;i<4;++i)
enum AxisEnum {X_AXIS,Y_AXIS,Z_AXIS,E_AXIS};
char axis_codes[]="XYZE";
struct PhysicalToolIndex {uint8_t i; auto to_raw() const {return i;}};
namespace stdext {template<class T> auto get_optional(std::optional<T> t) {return t;}}
struct Parser {bool t=false; int axis=0;
 bool seenval(char) {return t;}
 bool seen(char c) {axis=c=='X'?0:c=='Y'?1:c=='Z'?2:3;return true;}
 float value_axis_units(AxisEnum) {return 100+axis;}
} parser;
struct Planner {float a[5]={}, f[5]={};
 void set_max_acceleration(uint8_t i,float v) {a[i]=v;}
 void set_max_feedrate(uint8_t i,float v) {f[i]=v;}
} planner;
struct GcodeSuite {
 static inline std::optional<PhysicalToolIndex> target;
 static auto get_target_physical_from_command() {return target;}
 static void M201(); static void M203();
};
'''.replace('#include <optional>',
            '#define DISTINCT %d\n#include <optional>' % distinct)
            code += function(source,
                             'void GcodeSuite::M201()') + '\n' + function(
                                 source, 'void GcodeSuite::M203()') + r'''
int main() {
 GcodeSuite::M201(); GcodeSuite::M203();
 for(int i=0;i<4+DISTINCT;++i) {assert(planner.a[i]>0);assert(planner.f[i]>0);}
 planner={}; parser.t=true; GcodeSuite::M201();GcodeSuite::M203();
 for(int i=0;i<5;++i) {assert(planner.a[i]==0);assert(planner.f[i]==0);}
 GcodeSuite::target=PhysicalToolIndex{1}; GcodeSuite::M201();GcodeSuite::M203();
 assert(planner.a[0]==100 && planner.f[E_AXIS_N(1)]==103);
 if(DISTINCT) assert(planner.a[E_AXIS_N(0)]==0);
}
'''
            self.run_cpp(code)

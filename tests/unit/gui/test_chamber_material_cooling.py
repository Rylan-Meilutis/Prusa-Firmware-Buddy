"""Execute material policy and production RME setting guards without hardware."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
FEATURE_ROOT = ROOT / "src/feature" if (
    ROOT / "src/feature/xbuddy_extension/xbuddy_extension.cpp"
).exists() else ROOT / "src/common/feature"


class ChamberMaterialCooling(unittest.TestCase):

    def compile_run(self, source):
        with tempfile.TemporaryDirectory() as directory:
            source_path = Path(directory) / "test.cpp"
            source_path.write_text(source)
            binary = Path(directory) / "test"
            subprocess.run([
                "c++", "-std=c++20", "-fsanitize=address,undefined",
                "-fno-pie", "-no-pie", "-I" + str(ROOT / "src"),
                "-I" + str(ROOT / "src/common"),
                str(source_path), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_materials_and_all_pwm_values(self):
        self.compile_run(r'''
#include <feature/chamber/material_cooling_policy.hpp>
#include <cassert>
using namespace buddy::chamber_material;
int main() {
  for (auto name : {"PLA", "PETG", "PET", "TPU", "FLEX", "TPE"}) assert(permits_cooling(name));
  for (auto name : {"ABS", "ASA", "HIPS", "PC", "PA", "", "PLA-00C", "unknown"}) assert(!permits_cooling(name));
  for (unsigned pwm=0; pwm<256; ++pwm) {
    for (unsigned filter=0; filter<256; ++filter) {
      assert(normal_pwm(pwm,true,true,false,filter)==filter);
      assert(normal_pwm(pwm,true,true,true,filter)==pwm);
      assert(normal_pwm(pwm,false,true,false,filter)==pwm);
      assert(normal_pwm(pwm,true,false,false,filter)==pwm);
      assert(normal_pwm(pwm,true,true,true,filter,false)==filter);
    }
  }
}
''')

    def test_production_routing_and_emergency_order(self):
        source = (FEATURE_ROOT /
                  "xbuddy_extension/xbuddy_extension.cpp").read_text()
        self.assertLess(source.index("chamber_material::normal_pwm("),
                        source.index("chamber_cooling.apply_pwm_overrides("))
        self.assertIn(
            "filtration_backend == ChamberFiltrationBackend::xbe_filter_on_cooling_fans ? filtration_pwm.value : 0",
            source)
        self.assertIn("if (!marlin_server::serial_print_active())", source)
        self.assertIn("all &= chamber_material::permits_cooling", source)
        self.assertIn("filament_material_name(parameters)", source)
        self.assertIn(
            "filtration_fan_actual_pwm_ = std::max(filtration_fan_actual_pwm_, filtration_pwm)",
            source)

    def test_production_command_validation(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        start = source.index('  if (command.starts_with("@RME CHAMBER SET "))')
        end = source.index('  if (command != "@RME TUNE QUERY")', start)
        body = source[start:end]
        for xbe, filtration in ((0, 0), (0, 1), (1, 1)):
            self.compile_run(r'''
#include <cassert>
#include <string_view>
#include <optional>
#include <string>
#include <charconv>
#define HAS_XBUDDY_EXTENSION() XBE
#define HAS_CHAMBER_FILTRATION_API() FILTRATION
#define SERIAL_ECHOLNPGM(x) last=x
std::string_view last;
bool session=true, locked=false;
namespace serial_remote_control {bool session_active(){return session;}}
namespace printer_lock {bool locked(){return ::locked;}}
struct Setting {bool value=false; int writes=0; void set(bool v){value=v;++writes;}};
struct Store {Setting xbe_material_chamber_cooling, chamber_print_filtration_enable, chamber_post_print_filtration_enable;} store;
Store &config_store(){return store;}
namespace rme_protocol {
std::optional<unsigned> unsigned_number(std::string_view text, std::string_view key) {
  auto pos=text.find(key); if(pos==text.npos) return {};
  text.remove_prefix(pos+key.size()+1); unsigned v=0;
  auto result=std::from_chars(text.data(),text.data()+text.size(),v);
  if(result.ec!=std::errc()) return {}; return v;
}}
bool handle(std::string_view command) {
'''.replace("XBE\n",
            str(xbe) + "\n").replace("FILTRATION\n",
                                     str(filtration) + "\n") + body + r'''
return false;}
int main() {
  for(auto field : {"material_cooling", "print_filter", "post_filter"}) {
    // Construct inputs in tests only; production dispatch remains allocation-free.
    for(auto suffix : {"=2", "=-1", "=", "=01", "=1 extra=0", "=bad"}) {
      std::string cmd="@RME CHAMBER SET ";cmd+=field;cmd+=suffix;
      assert(handle(cmd)); assert(last.find("ERROR")!=last.npos);
    }
  }
  assert(store.xbe_material_chamber_cooling.writes==0);
  assert(store.chamber_print_filtration_enable.writes==0);
  assert(store.chamber_post_print_filtration_enable.writes==0);
  handle("@RME CHAMBER SET material_cooling=1");
  assert(store.xbe_material_chamber_cooling.writes==HAS_XBUDDY_EXTENSION());
  handle("@RME CHAMBER SET print_filter=1");
  assert(store.chamber_print_filtration_enable.writes==HAS_CHAMBER_FILTRATION_API());
  handle("@RME CHAMBER SET post_filter=1");
  assert(store.chamber_post_print_filtration_enable.writes==HAS_CHAMBER_FILTRATION_API());
  session=false;handle("@RME CHAMBER SET material_cooling=0");
  session=true;locked=true;handle("@RME CHAMBER SET material_cooling=0");
  assert(store.xbe_material_chamber_cooling.writes==HAS_XBUDDY_EXTENSION());
}
'''.replace("#include <cassert>", "#include <cassert>\n#include <string>"))

    def test_production_fan_outputs(self):
        source = (FEATURE_ROOT /
                  "xbuddy_extension/xbuddy_extension.cpp").read_text()
        start = source.index("        switch (filtration_backend) {")
        end = source.index("        const auto set_fan_pwm", start)
        body = source[start:end]
        self.compile_run(r'''
#include <feature/chamber/material_cooling_policy.hpp>
#include <algorithm>
#include <optional>
#include <cassert>
#include <cstdint>
using namespace buddy;
struct FanPWM {uint8_t value=0;};
using Optional = std::optional<FanPWM>;
constexpr auto pwm_auto=std::nullopt;
bool operator<(FanPWM a,FanPWM b){return a.value<b.value;}
enum class ChamberFiltrationBackend {xbe_official_filter, xbe_filter_on_cooling_fans, none};
struct Setting {int value; int get()const{return value;}};
struct Store {Setting xbe_cooling_fans_with_filter{0}, xbe_filtration_fan_print_offset_pct{30};} store;
Store &config_store(){return store;}
bool job;
namespace marlin_server {bool is_printing(){return job;}}
FanPWM add_pwm_offset(FanPWM pwm,int offset){return {uint8_t(std::min(255,int(pwm.value)+offset))};}
struct Cooling {
 bool emergency=false;
 FanPWM compute_pwm_step(float,float,Optional target,FanPWM maximum){return target.value_or(maximum);}
 FanPWM apply_pwm_overrides(bool,FanPWM pwm){return emergency?FanPWM{255}:pwm;}
} chamber_cooling;
int main(){
 for(auto filtration_backend : {ChamberFiltrationBackend::xbe_official_filter, ChamberFiltrationBackend::xbe_filter_on_cooling_fans, ChamberFiltrationBackend::none})
 for(bool material_cooling : {false,true})
 for(bool printing : {false,true})
 for(bool material_permits_cooling : {false,true})
 for(bool emergency : {false,true})
 for(bool manual : {false,true})
 for(bool fans_enabled : {false,true})
 for(int minimum : {0,100,255}) {
   store.xbe_cooling_fans_with_filter.value=fans_enabled; job=printing;
   chamber_cooling.emergency=emergency;
   FanPWM cooling_fans_actual_pwm_,filtration_fan_actual_pwm_;
   FanPWM filtration_pwm{uint8_t(minimum)};
   FanPWM max_auto_pwm{102}, max_cooling_fans_auto_pwm{102}, max_filtration_fan_auto_pwm{178};
   Optional cooling_fans_target_pwm_=manual?Optional{FanPWM{220}}:Optional{};
   Optional filtration_fan_target_pwm_{};
   Optional rpm0{FanPWM{100}},rpm1{FanPWM{100}},rpm2{FanPWM{100}};
   // Production RPM optionals contain integers, not PWM objects.
   std::optional<unsigned> rpm0n=100,rpm1n=100,rpm2n=100;
   bool can_auto_cool_=false;
   std::optional<float> temp=50;float target_temp=40;
''' + body.replace("rpm0.value_or", "rpm0n.value_or").replace(
            "rpm1.value_or", "rpm1n.value_or").replace("rpm2.value_or",
                                                       "rpm2n.value_or") + r'''
   if(emergency) {assert(cooling_fans_actual_pwm_.value==255);assert(filtration_fan_actual_pwm_.value==255);}
   else if(material_cooling && printing && (!material_permits_cooling || (!fans_enabled && filtration_backend==ChamberFiltrationBackend::xbe_official_filter))) {
     assert(cooling_fans_actual_pwm_.value==(filtration_backend==ChamberFiltrationBackend::xbe_filter_on_cooling_fans?minimum:0));
   }
   if(filtration_backend==ChamberFiltrationBackend::xbe_official_filter) assert(filtration_fan_actual_pwm_.value>=minimum);
 }
}
''')

    def test_production_material_selection(self):
        source = (FEATURE_ROOT /
                  "xbuddy_extension/xbuddy_extension.cpp").read_text()
        start = source.index("    const bool material_cooling =")
        end = source.index("    std::lock_guard _lg(mutex_);", start)
        self.compile_run(r'''
#include <feature/chamber/material_cooling_policy.hpp>
#include <cassert>
#include <string_view>
#include <vector>
using namespace buddy;
using GcodeToolIndex=int;using VirtualToolIndex=int;
bool enabled=true, printing=true, serial=false;
struct Setting {bool get(){return enabled;}};
struct {Setting xbe_material_chamber_cooling;} store;
auto &config_store(){return store;}
struct {struct {int get(){return 0;}} print_state;} vars;
auto &marlin_vars(){return vars;}
namespace marlin_server {
 bool is_filtration_job_active(int){return printing;}
 bool serial_print_active(){return serial;}
}
struct Parameters {std::string_view name;};
std::string_view selected="ASA";
struct FilamentType {
 std::string_view family;
 explicit operator bool()const{return !family.empty();}
 Parameters parameters()const{return {family};}
 static FilamentType from_name(std::string_view name){
   if(name=="PLA-CUSTOM")return {"PLA"};
   if(name=="PLA"||name=="ABS"||name=="ASA"||name=="PETG"||name=="FLEX")return {name};
   return {};
 }
 static FilamentType for_tool_heuristic(int){return {selected};}
 static FilamentType for_current_tool_heuristic(){return {selected};}
};
std::string_view filament_material_name(Parameters p){return p.name;}
struct Info {std::string_view filament_name;};
struct GCodeInfo {
 std::vector<Info> entries;
 static GCodeInfo &getInstance(){static GCodeInfo info;return info;}
 template<class F>void for_each_used_extruder(F callback){for(auto info:entries)callback(0,0,info);}
};
bool run(){
''' + source[start:end] + r'''
return material_permits_cooling;}
int main(){
 auto &entries=GCodeInfo::getInstance().entries;
 for(auto material:{"PLA","PETG","TPU","FLEX","TPE","PET","PLA-CUSTOM"}) {
   entries={{material}}; assert(run());
 }
 for(auto material:{"ABS","ASA","HIPS","PC","PA","unknown"}) {
   selected="PLA"; entries={{material}};assert(!run());
   entries={{"PLA"},{material}};assert(!run());
 }
 entries={{""}};selected="PETG";assert(run());
 entries={{"ABS"}};serial=true;selected="PLA";assert(!run());
 chamber_material::serial_job.report(true);assert(run());
 entries={{"PLA"}};selected="ASA";assert(!run());
 selected="PLA";assert(!run()); // Never re-enable after a forbidden tool.
 chamber_material::serial_job.report(true);assert(!run());
 chamber_material::serial_job.reset();assert(!run());
 chamber_material::serial_job.report(false);assert(!run());
 chamber_material::serial_job.report(true);assert(!run());
 chamber_material::serial_job.reset();chamber_material::serial_job.report(true);assert(run());
 entries={};selected="";assert(!run());
 enabled=false;selected="PLA";assert(!run());
}
''')

    def test_production_job_command_and_reset_boundary(self):
        source = (ROOT / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        start = source.index('  if (command.starts_with("@RME CHAMBER JOB "))')
        end = source.index('  if (command.starts_with("@RME CHAMBER SET "))',
                           start)
        self.compile_run(r'''
#include <feature/chamber/material_cooling_policy.hpp>
#include <cassert>
#include <string_view>
#define HAS_XBUDDY_EXTENSION() 1
#define SERIAL_ECHOLNPGM(x) last=x
std::string_view last;
bool session=true,locked=false,active=true;
namespace serial_remote_control {bool session_active(){return session;}}
namespace printer_lock {bool locked(){return ::locked;}}
namespace marlin_server {enum class State{Idle,SerialPrintInit};bool serial_print_active(){return active;}}
marlin_server::State state=marlin_server::State::Idle;
struct {struct {auto get(){return state;}} print_state;} vars;
auto &marlin_vars(){return vars;}
bool handle(std::string_view command){
''' + source[start:end] + r'''
return false;}
int main(){
 using namespace buddy::chamber_material;
 for(auto bad:{"@RME CHAMBER JOB allow_cooling=2","@RME CHAMBER JOB allow_cooling=1 extra=0","@RME CHAMBER JOB allow_cooling="}){
   handle(bad);assert(!serial_job.permits());assert(last.find("ERROR")!=last.npos);
 }
 session=false;handle("@RME CHAMBER JOB allow_cooling=1");assert(!serial_job.permits());
 session=true;locked=true;handle("@RME CHAMBER JOB allow_cooling=1");assert(!serial_job.permits());
 locked=false;active=false;handle("@RME CHAMBER JOB allow_cooling=1");assert(!serial_job.permits());
 state=marlin_server::State::SerialPrintInit;handle("@RME CHAMBER JOB allow_cooling=1");assert(serial_job.permits());
 handle("@RME CHAMBER JOB allow_cooling=0");assert(!serial_job.permits());
 handle("@RME CHAMBER JOB allow_cooling=1");assert(!serial_job.permits());
 serial_job.reset();active=true;handle("@RME CHAMBER JOB allow_cooling=1");assert(serial_job.permits());
}
''')
        server = (ROOT / "src/common/marlin_server.cpp").read_text()
        self.assertIn(
            "if (server.print_state != State::SerialPrintInit) {\n        buddy::chamber_material::serial_job.reset();",
            server)


if __name__ == "__main__":
    unittest.main()

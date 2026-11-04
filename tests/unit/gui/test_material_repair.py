"""Legacy production assignment, inline EEPROM storage and material report."""
import unittest
import test_serial_pause_recovery as fixture


class MaterialRepair(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_exact_profile_repair_persists_without_changing_color_or_brand(
            self):
        source = (fixture.ROOT /
                  'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        assign = source.split('} else if (action.starts_with("ASSIGN")) {',
                              1)[1].split(
                                  '} else if (action.starts_with("SET")', 1)[0]
        eeprom = (fixture.ROOT / 'src/common/filament_eeprom.hpp').read_text()
        decode = fixture.function(
            eeprom,
            'FilamentTypeParameters::BasePreset decode_inline_base_preset()')
        encode = fixture.function(eeprom,
                                  'static uint8_t encode_inline_base_preset(')
        display = fixture.function(
            (fixture.ROOT / 'src/common/filament.cpp').read_text(),
            'FilamentTypeParameters::Name filament_material_name(')
        self.run_cpp(r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <optional>
#include <sstream>
#include <variant>
#include <filament_material.hpp>
#include <filament_material_family_storage.hpp>
#include <rme_protocol_parser.hpp>
#define HAS_FILAMENT_MATERIAL_FAMILY_PARAM() 1
#define EXTRUDERS 8
enum class PresetFilamentType { PLA,PETG,ASA,ABS,FLEX,PC,PA,PVB,HIPS,PP,PVA,_count };
constexpr std::array<const char*,11> names={"PLA","PETG","ASA","ABS","FLEX","PC","PA","PVB","HIPS","PP","PVA"};
struct FilamentTypeParameters {
 using Name=std::array<char,8>; using BasePreset=std::optional<PresetFilamentType>;
 Name name{}; BasePreset base_preset; int nozzle=220,bed=60;
};
struct EEPROM { uint8_t _unused=0;
''' + decode + '\n' + encode + r'''
} eeprom;
struct Presets { std::array<FilamentTypeParameters,11> values;
 const auto &operator[](PresetFilamentType n) const {return values[static_cast<size_t>(n)];}
} preset_filament_parameters;
''' + display + r'''
FilamentTypeParameters stored;
uint32_t color=0x123456; std::string brand="Prusament";
int writes=0; bool locked=false,idle=true,active=false,paused=false;
std::ostringstream wire;
#define SERIAL_ECHO(x) wire << x
#define SERIAL_ECHOLN(x) wire << x << '\n'
#define SERIAL_ECHOLNPGM(x) wire << x << '\n'
struct FilamentType {
 int id=1; std::optional<PresetFilamentType> preset; static FilamentType none;
 static FilamentType from_name(std::string_view name) {
  for(size_t i=0;i<names.size();++i) if(name==names[i]) return {int(i)+2,static_cast<PresetFilamentType>(i)};
  return none;
 }
 auto parameters() const {auto p=stored;p.base_preset=eeprom.decode_inline_base_preset();return p;}
 bool is_customizable() const {return id==1;}
 void set_parameters(FilamentTypeParameters p) {++writes;stored=p;eeprom._unused=EEPROM::encode_inline_base_preset(p.base_preset);}
 bool operator==(const FilamentType &b) const {return id==b.id;}
 bool operator!=(const FilamentType &b) const {return id!=b.id;}
};
FilamentType FilamentType::none{0,{}};
namespace std {template<class T> const T *get_if(const FilamentType *t) {return t->preset ? &*t->preset : nullptr;}}
struct Store {
 FilamentType get_filament_type(uint8_t tool) {assert(tool==7);return {};}
 void set_filament_type(uint8_t tool,FilamentType type) {assert(tool==7 && type.id==1);}
} store;
auto &config_store(){return store;}
namespace printer_lock {bool locked(){return ::locked;}}
namespace marlin_server {bool printer_idle(){return idle;} bool serial_print_active(){return active;} bool printer_paused_extended(){return paused;}}
namespace SerialPrinting {void notify_configuration(const char*,const char*,bool,unsigned){}}
auto remote_number(std::string_view c,std::string_view k){return rme_protocol::signed_number(c,k);}
auto remote_value(std::string_view c,std::string_view k){return rme_protocol::value(c,k);}
std::optional<unsigned> remote_transaction(std::string_view){return {};}
bool assign(std::string_view command) {
''' + assign + r'''
return true;
}
int main() {
 for(size_t i=0;i<names.size();++i) strcpy(preset_filament_parameters.values[i].name.data(),names[i]);
 for(auto profile:{"PLA-00C","TPU-00L","S00ABCD","PC-ZZZ","CUSTOM"}) for(auto material:names) {
  strcpy(stored.name.data(),profile);eeprom._unused=0;
  assign(std::string("@RME FILAMENT ASSIGN tool=7 if_profile=")+profile+" material="+material);
  auto reloaded=store.get_filament_type(7).parameters();
  assert(std::string_view(filament_material_name(reloaded).data())==material);
  assert(std::string_view(reloaded.name.data())==profile && reloaded.nozzle==220 && reloaded.bed==60);
  assert(color==0x123456 && brand=="Prusament");
  assert(!buddy::filament_material::matches(profile,material));
 }
 strcpy(stored.name.data(),"S00ABCD");auto before=writes;
 assign("@RME FILAMENT ASSIGN tool=7 if_profile=OTHER material=PLA");assert(writes==before);
 assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=INVALID");assert(writes==before);
 assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD profile=PLA material=PLA");assert(writes==before);
 active=true;assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=PLA");assert(writes==before);active=false;
 paused=true;assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=PLA");assert(writes==before);paused=false;
 idle=false;assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=PLA");assert(writes==before);
}
''')

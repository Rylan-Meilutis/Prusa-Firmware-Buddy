"""Execute production material assignment and EEPROM family serialization."""
import unittest
import test_serial_pause_recovery as fixture


class FilamentMaterialAssignment(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_explicit_family_survives_profile_assignment_and_reload(self):
        source = (fixture.ROOT /
                  "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        body = source.split('} else if (action.starts_with("ASSIGN")) {',
                            1)[1].split('} else if (action.starts_with("SET")',
                                        1)[0]
        eeprom = (fixture.ROOT / "src/common/filament_eeprom.hpp").read_text()
        storage = fixture.function(
            eeprom,
            "struct __attribute__((packed)) FilamentTypeParameters_EEPROM4"
        ) + ";"
        self.run_cpp(r'''
#include <array>
#include <cassert>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <type_traits>
#include <filament_material.hpp>
#include <m976_material.hpp>
#include <rme_protocol_parser.hpp>
#define HAS_FILAMENT_MATERIAL_FAMILY_PARAM() 1
namespace std { template<class E> constexpr auto to_underlying(E v) {return static_cast<std::underlying_type_t<E>>(v);} }
enum class PresetFilamentType { PLA, PETG, ASA, ABS, FLEX, PC, PA, PVB, HIPS, PP, PVA, _count, _count_sparse=_count };
constexpr std::array<std::string_view,11> names={"PLA","PETG","ASA","ABS","FLEX","PC","PA","PVB","HIPS","PP","PVA"};
struct FilamentTypeParameters { using BasePreset=std::optional<PresetFilamentType>; };
struct EncodedFilamentType {
    static std::optional<PresetFilamentType> preset_filament_type_from_enum_value(uint8_t n) {
        return n<names.size() ? std::optional{static_cast<PresetFilamentType>(n)} : std::nullopt;
    }
};
''' + storage + r'''
struct Params { std::string name="S00ABCD"; std::optional<PresetFilamentType> base_preset;
    int nozzle=220, bed=60; };
struct Stored { Params params; FilamentTypeParameters_EEPROM4 family; } stored;
int writes=0, selected=-1;
bool locked=false, idle=true, active=false, paused=false;
std::ostringstream wire;
#define SERIAL_ECHO(x) wire << x
#define SERIAL_ECHOLN(x) wire << x << '\n'
#define SERIAL_ECHOLNPGM(x) wire << x << '\n'
struct FilamentType {
    int id=1; std::optional<PresetFilamentType> preset;
    static FilamentType none;
    static FilamentType from_name(std::string_view name) {
        for(size_t i=0;i<names.size();++i) if(name==names[i]) return {int(i)+2,static_cast<PresetFilamentType>(i)};
        return none;
    }
    Params parameters() const { auto p=stored.params; p.base_preset=stored.family.decode_base_preset(); return p; }
    bool is_customizable() const { return !preset && id==1; }
    void set_parameters(Params p) { ++writes; stored.params=p;
        stored.family.base_preset=FilamentTypeParameters_EEPROM4::encode_base_preset(p.base_preset); }
    bool operator==(const FilamentType &b) const {return id==b.id;}
    bool operator!=(const FilamentType &b) const {return !(*this==b);}
};
FilamentType FilamentType::none{0,{}};
namespace std { template<class T> const T *get_if(const FilamentType *t) {return t->preset ? &*t->preset : nullptr;} }
struct VirtualToolIndex { static constexpr int count=8; int n;
    static auto from_raw(int n) {return VirtualToolIndex{n};} };
struct Store {
    FilamentType get_filament_type(VirtualToolIndex tool) {assert(tool.n==7); return {};}
    void set_filament_type(VirtualToolIndex tool, FilamentType type) {assert(type.id==1); selected=tool.n;}
} store;
Store &config_store() {return store;}
namespace printer_lock { bool locked() {return ::locked;} }
namespace marlin_server {
bool printer_idle() {return idle;}
bool serial_print_active() {return active;}
bool printer_paused_extended() {return paused;}
}
namespace SerialPrinting {void notify_configuration(const char*,const char*,bool,unsigned){}}
auto remote_number(std::string_view c,std::string_view k) {return rme_protocol::signed_number(c,k);}
auto remote_value(std::string_view c,std::string_view k) {return rme_protocol::value(c,k);}
std::optional<unsigned> remote_transaction(std::string_view){return {};}
struct Name { std::string value; const char *data() const {return value.c_str();} };
Name filament_material_name(const Params &p) {
    auto base=p.base_preset ? names[static_cast<size_t>(*p.base_preset)] : std::string_view{};
    return {std::string(buddy::filament_material::authoritative_name(p.name,base))};
}
bool assign(std::string_view command) {
''' + body + r'''
return true;
}
int main() {
    // Includes deliberately misleading and opaque identities: no prefix rule.
    for (auto profile : {"PLA-00C","TPU-00L","PC-ZZZ","S00ABCD","CUSTOM"}) {
        for (auto material : names) {
            stored.params.name=profile; stored.family.base_preset=0xff;
            const auto command=std::string("@RME FILAMENT ASSIGN tool=7 if_profile=")+profile+" material="+std::string(material);
            auto before=writes; assign(command); assert(writes==before+1 && selected==7);
            const auto reloaded=store.get_filament_type({7}).parameters();
            assert(reloaded.name==profile && reloaded.nozzle==220 && reloaded.bed==60);
            assert(filament_material_name(reloaded).value==material);
            assert(buddy::m976_material::matches(material, reloaded.name, filament_material_name(reloaded).value));
            assert(!buddy::m976_material::matches(profile, reloaded.name, filament_material_name(reloaded).value));
        }
    }
    stored.params.name="S00ABCD";
    const auto before=writes;
    for (auto command : {"tool=7 if_profile=OTHER material=PLA", "tool=7 if_profile=S00ABCD material=INVALID",
                         "tool=7 if_profile=S00ABCD profile=PLA material=PLA"}) {
        assign(std::string("@RME FILAMENT ASSIGN ")+command); assert(writes==before);
    }
    active=true; assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=PLA"); assert(writes==before); active=false;
    paused=true; assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=PLA"); assert(writes==before); paused=false;
    idle=false; assign("@RME FILAMENT ASSIGN tool=7 if_profile=S00ABCD material=PLA"); assert(writes==before);
}
''')

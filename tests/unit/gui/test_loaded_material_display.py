"""Family aliases and the production mapping-screen loaded-data selection."""
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]


class LoadedMaterialDisplayTests(unittest.TestCase):

    def test_production_profile_match_accepts_explicit_base_material(self):
        source = (ROOT / "src/common/filament.cpp").read_text()
        start = source.index("bool FilamentType::matches(")
        body = source[start:source.index("\n}", start) + 2]
        self.compile_run(r'''
#include <filament_material.hpp>
#include <cassert>
#include <string>
struct Params {std::string name; std::string base;};
Params current;
std::string filament_material_name(Params p) {
 return std::string(buddy::filament_material::authoritative_name(p.name,p.base));
}
struct FilamentType {
 Params parameters() const {return current;}
 bool matches(const std::string_view &name) const;
};
''' + body + r'''
int main() {
 FilamentType loaded;
 current={"TPU-00L","FLEX"};
 assert(loaded.matches("TPU-00L")); assert(loaded.matches("TPU"));
 assert(loaded.matches("TPE")); assert(loaded.matches("FLEX"));
 assert(!loaded.matches("PLA")); assert(!loaded.matches("TPU-95A"));
 current={"ASA-00A","ASA"}; assert(loaded.matches("ASA"));
 assert(!loaded.matches("ABS"));
 current={"TPU-00L",""}; assert(!loaded.matches("TPU"));
 current={"PLA-CF",""}; assert(!loaded.matches("PLA"));
}
''')

    def compile_run(self, source):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            cpp.write_text(source)
            subprocess.run([
                compiler, "-std=c++23", "-fsanitize=address,undefined", "-I",
                str(ROOT / "src/common"),
                str(cpp), "-o",
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)],
                           check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_material_aliases_without_weakening_specialty_checks(self):
        self.compile_run(r'''
#include <filament_material.hpp>
#include <cassert>
using namespace buddy::filament_material;
int main() {
 for (auto name : {"TPU", "FLEX", "TPE"}) {
  assert(matches(name, "FLEX")); assert(matches("FLEX", name));
 }
 assert(matches("PET", "PETG")); assert(matches("NYLON", "PA"));
 assert(matches("ASA", "ASA")); assert(!matches("ASA", "ABS"));
 for (auto name : {"TPU-95A", "TPU-00L", "PLA-CF", "PLA-00D", "", "---"}) {
  assert(!matches(name, "FLEX")); assert(!matches(name, "PLA"));
 }
 assert(!matches("", "")); assert(!matches("---", "---"));
 assert(matches("TPU", authoritative_name("TPU-00L", "FLEX")));
 assert(!matches("TPU", authoritative_name("TPU-00L", "")));
}
''')
        checks = ROOT / "src/feature/compatibility_checks/gcode_compatibility.cpp"
        if checks.exists():
            source = checks.read_text()
            self.assertIn("filament_material::matches", source)
            self.assertNotIn("fn != filament_material_name", source)

    def test_production_loaded_mapping_row_uses_material_and_stored_color(
            self):
        path = ROOT / "src/gui/screen/print/frame_tool_mapping.cpp"
        if not path.exists():
            self.skipTest("This release uses the legacy mapping UI")
        source = path.read_text()
        start = source.index(
            "        const auto physical_tool = virtual_tool->to_physical();")
        end = source.index("        break;", start)
        block = source[start:end]
        self.compile_run(r'''
#include <cassert>
#include <optional>
#include <string>
#include <cstdint>
using Color=uint32_t;
constexpr Color COLOR_NONE=0xffffffff;
template<class T, auto none> using CompactOptional=std::optional<T>;
constexpr float NAN=0;
struct Params {std::string name="TPU-00L";};
struct Filament {Params parameters(){return {};}};
struct Store {Filament get_filament_type(int){return {};} float get_nozzle_diameter(int){return 0.4f;}};
Store &config_store(){static Store store;return store;}
std::string filament_material_name(Params){return "FLEX";}
struct Tool {
 int to_physical(){return 6;} int to_raw(){return 4;}
 std::string compact_display_name(int){return "T4";}
 operator int() const {return 4;}
} tool;
namespace filament_color {
std::optional<Color> current;
std::optional<Color> loaded(int slot){assert(slot==4);return current;}
}
struct Data {std::string tool_name;std::optional<float> nozzle_diameter;
 std::string filament_name;std::optional<Color> color;};
Data row() {
 Data data; auto *virtual_tool=&tool; int tool_name_buf=0;
''' + block + r'''
 return data;
}
int main() {
 for(auto rgb:{0x000000u,0xffffffu,0x0073ffu}) {
  filament_color::current=rgb;
  auto data=row();assert(data.color && *data.color==rgb);
  assert(data.filament_name=="FLEX");
 }
 filament_color::current={}; assert(!row().color);
}
''')

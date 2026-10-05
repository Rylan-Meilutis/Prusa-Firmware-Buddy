"""Execute the production PA chain expansion with lightweight host stubs."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]


class SpoolJoinPreparation(unittest.TestCase):

    def test_batch_expansion(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        function = source.split("bool expand_spool_join_batch(", 1)[1].split(
            "\n#if HAS_INDX()\nbuddy::pa_cache::Key", 1)[0]
        offsets = (
            ROOT /
            "src/feature/tool_offset_calibration/tool_offset_calibration.cpp"
        ).read_text()
        offsets = offsets.split("// Explicit serial-print tool lists", 1)[1]
        offsets = offsets[offsets.index("#if HAS_SPOOL_JOIN()"):].split(
            "#endif", 1)[0] + "#endif\n"
        stub = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <optional>
#include <cstdint>
#define HAS_SPOOL_JOIN() 1
#define HAS_INDX() 1
#define SERIAL_ECHOLNPAIR(...) ((void)0)
#define log_error(...) ((void)0)
enum class Context { Print, Calibration };
namespace buddy::extrusion_calibration { constexpr size_t max_logical_filaments=8; }
namespace buddy::m976_material {
bool matches(const char *a, const char *b, const char *) { return !strcmp(a,b); }
}
struct BatchEntry { uint8_t physical_tool, logical_filament; int16_t temperature;
    std::array<char,17> material{}; };
std::array<bool,8> enabled;
std::array<int,8> links;
struct VirtualToolIndex {
    int n;
    static constexpr int count=8;
    static std::array<VirtualToolIndex,8> all();
    static auto from_raw(int n) { return VirtualToolIndex{n}; }
    int to_raw() const { return n; }
    bool is_enabled() const { return enabled[n]; }
    auto to_physical() const { return *this; }
};
std::array<VirtualToolIndex,8> VirtualToolIndex::all() {
    return {VirtualToolIndex{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
}
struct Params { std::array<char,17> name{}; };
const char *base_material_name(const Params &) { return ""; }
struct FilamentType {
    Params p;
    static constexpr int none=0;
    bool operator==(int) const { return !p.name[0]; }
    Params parameters() const { return p; }
};
struct Store {
    std::array<FilamentType,8> filaments;
    auto get_filament_type(VirtualToolIndex t) { return filaments[t.n]; }
} store;
Store &config_store() { return store; }
struct Join {
    std::optional<VirtualToolIndex> get_spool_2(VirtualToolIndex t) {
        if (links[t.n]<0) return {};
        return VirtualToolIndex{links[t.n]};
    }
} spool_join;
'''
        cases = r'''
int main() {
    using Entries=std::array<BatchEntry,8>;
    auto reset=[] { links.fill(-1); enabled.fill(true);
        for (auto &f:store.filaments) strcpy(f.p.name.data(),"PLA"); };
    auto root=[](int n) { BatchEntry e{}; e.physical_tool=e.logical_filament=n;
        e.temperature=220; strcpy(e.material.data(),"PLA"); return e; };
    reset(); Entries e{}; e[0]=root(4); size_t count=1;
    assert(expand_spool_join_batch(e,count) && count==1);
    links[4]=2; links[2]=7; links[0]=1;
    assert(expand_spool_join_batch(e,count) && count==3);
    assert(e[1].logical_filament==2 && e[2].physical_tool==7);
    assert(e[2].temperature==220 && !strcmp(e[2].material.data(),"PLA"));
    assert(expand_spool_join_batch(e,count) && count==3); // no duplicates
    reset(); e={}; e[0]=root(4); e[1]=root(7); count=2;
    links[4]=2; links[2]=7;
    assert(expand_spool_join_batch(e,count) && count==3); // explicit backup
    reset(); e={}; e[0]=root(4); count=1; links[4]=2; enabled[2]=false;
    assert(!expand_spool_join_batch(e,count));
    enabled[2]=true; store.filaments[2].p.name[0]=0;
    assert(!expand_spool_join_batch(e,count));
    strcpy(store.filaments[2].p.name.data(),"PETG");
    assert(!expand_spool_join_batch(e,count));
    reset(); e={}; e[0]=root(0); count=1;
    for (int i=0;i<7;++i) links[i]=i+1;
    assert(expand_spool_join_batch(e,count) && count==8);
    // Even damaged cyclic state cannot loop or overrun the bounded array.
    links[7]=0;
    assert(expand_spool_join_batch(e,count) && count==8);
    reset(); links[7]=4; links[4]=1; links[0]=2;
    std::optional<uint32_t> mask=1u<<7;
    assert(expand_offsets(mask,Context::Print));
    assert(*mask==((1u<<7)|(1u<<4)|(1u<<1))); // reverse-ordered chain
    mask=1u<<7;
    assert(expand_offsets(mask,Context::Calibration) && *mask==(1u<<7));
    enabled[4]=false;
    assert(!expand_offsets(mask,Context::Print));
    mask={};
    assert(expand_offsets(mask,Context::Print) && !mask);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / "test.cpp"
            path.write_text(
                stub + "bool expand_spool_join_batch(" + function +
                "bool expand_offsets(std::optional<uint32_t> &physical_tool_mask, Context context) {\n"
                + offsets + "return true;\n}\n" + cases)
            exe = pathlib.Path(tmp) / "test"
            subprocess.run([
                "c++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                str(path), "-o",
                str(exe)
            ],
                           check=True)
            subprocess.run([str(exe)], check=True)

    def test_expansion_precedes_cache_and_motion(self):
        source = (ROOT / "src/marlin_stubs/M976.cpp").read_text()
        batch = source.split("if (parser.seen('A') || parser.seenval('K'))",
                             1)[1]
        self.assertLess(batch.index("expand_spool_join_batch(entries, count)"),
                        batch.index("restore_cache("))
        self.assertLess(batch.index("expand_spool_join_batch(entries, count)"),
                        batch.index("run_batch("))
        self.assertIn("const bool force = manual || mode == 2", source)
        self.assertIn("if (!manual && mode == 0", source)

    def test_explicit_offsets_expand_before_selecting_enabled_tools(self):
        source = (
            ROOT /
            "src/feature/tool_offset_calibration/tool_offset_calibration.cpp"
        ).read_text()
        section = source.split("PhysicalToolSet used_physical_tools;", 1)[1]
        self.assertIn("context == Context::Print", section)
        self.assertIn("pass < VirtualToolIndex::count", section)
        self.assertLess(section.index("spool_join.get_spool_2(tool)"),
                        section.index("used_physical_tools.set"))


if __name__ == "__main__":
    unittest.main()

"""Exercise the production query selector against the actual sparse preset IDs."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class SparseFilamentQueryTests(unittest.TestCase):

    def test_query_enumerates_existing_presets_and_all_user_profiles(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        header = (root / "src/common/filament.hpp").read_text()
        enum_start = header.index("enum class PresetFilamentType")
        enum = header[enum_start:header.index("};", enum_start) + 2]
        array_start = header.index(
            "inline constexpr std::array preset_filament_types")
        presets = header[array_start:header.index("};", array_start) + 2]
        source = (root / "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        function = source.split("static void report_remote_filaments() {",
                                1)[1]
        selection = function.split(
            "    const auto params = type.parameters();", 1)[0]
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            executable = pathlib.Path(directory) / "test"
            for high_temperature in (0, 1):
                with self.subTest(high_temperature=high_temperature):
                    cpp.write_text(r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>
#include <algorithm>
''' + "#define HAS_HT_HOTEND() " + str(high_temperature) + "\n" + enum + "\n" +
                                   presets + r'''
constexpr size_t preset_filament_type_count = preset_filament_types.size();
constexpr size_t user_filament_type_count = 8;
struct UserFilamentType { uint8_t index; };
struct FilamentType {
    bool user;
    uint8_t id;
    FilamentType(UserFilamentType t) : user(true), id(t.index) {}
    FilamentType(PresetFilamentType t) : user(false), id(std::to_underlying(t)) {}
};
int main() {
size_t seen_presets = 0, seen_users = 0;
''' + selection + r'''
    assert(type.user == user);
    assert(type.id == slot);
    if (user) {
        assert(type.id == seen_users++);
    } else {
        // Invalid IDs must never reach parameters(): its compact preset
        // table cannot be indexed by sparse enum holes such as 10 and 11.
        assert(type.id == std::to_underlying(preset_filament_types[seen_presets++]));
        assert(std::find(preset_filament_types.begin(), preset_filament_types.end(),
                         static_cast<PresetFilamentType>(type.id)) != preset_filament_types.end());
    }
}
assert(seen_presets == preset_filament_type_count);
assert(seen_users == user_filament_type_count);
}
''')
                    subprocess.run([
                        compiler, "-std=c++23",
                        str(cpp), "-o",
                        str(executable)
                    ],
                                   check=True,
                                   capture_output=True)
                    subprocess.run([str(executable)],
                                   check=True,
                                   capture_output=True,
                                   cwd=directory)


if __name__ == "__main__":
    unittest.main()

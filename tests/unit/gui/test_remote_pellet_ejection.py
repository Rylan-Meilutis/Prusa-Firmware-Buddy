"""Execute production cleaner setting guards and delta readback without hardware."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class RemotePelletEjection(unittest.TestCase):

    def test_production_setter_and_capability(self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        start = source.index('  if (command.starts_with("@RME CLEANER SET"))')
        end = source.index('  if (command.starts_with("@RME CHAMBER JOB "))',
                           start)
        block = source[start:end]
        code = r'''
#include <cassert>
#include <string>
#include <string_view>
bool session=true, locked=false;
namespace serial_remote_control {bool session_active(){return session;}}
namespace printer_lock {bool locked(){return ::locked;}}
struct Setting {bool value=false; int writes=0; void set(bool v){value=v; ++writes;}};
struct Store {Setting indx_entry_only_pellet_ejection;} store;
Store &config_store(){return store;}
std::string report;
#define SERIAL_ECHOLNPGM(x) (report=(x))
bool handle(std::string_view command) {
''' + block + r'''
return false;
}
int main() {
 for (bool active : {false,true}) for (bool lock : {false,true}) {
  session=active; locked=lock; report.clear();
  int writes=store.indx_entry_only_pellet_ejection.writes;
  assert(handle("@RME CLEANER SET entry_only=1"));
  if (!active || lock) {
   assert(report.find("locked_or_no_session")!=std::string::npos);
   assert(store.indx_entry_only_pellet_ejection.writes==writes);
  }
 }
 session=true; locked=false;
#if RME_HAS_INDX()
 for (bool value : {false,true,false}) {
  assert(handle(value?"@RME CLEANER SET entry_only=1":"@RME CLEANER SET entry_only=0"));
  assert(store.indx_entry_only_pellet_ejection.value==value);
  assert(report=="RME_CLEANER accepted=1");
 }
 int writes=store.indx_entry_only_pellet_ejection.writes;
 for (auto suffix : {"", " entry_only=2", " entry_only=-1", " entry_only=01", " entry_only=0 extra=1", " entry_only=1\nM112", " typo=1"}) {
  assert(handle(std::string("@RME CLEANER SET")+suffix));
  assert(report.find("invalid_argument")!=std::string::npos);
  assert(store.indx_entry_only_pellet_ejection.writes==writes);
 }
#else
 assert(handle("@RME CLEANER SET entry_only=0"));
 assert(report.find("unsupported")!=std::string::npos);
 assert(store.indx_entry_only_pellet_ejection.writes==0);
#endif
 assert(!handle("@RME TUNE QUERY"));
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            (path / 'test.cpp').write_text(code)
            for indx in (0, 1):
                subprocess.run([
                    'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-DRME_HAS_INDX()=' + str(indx),
                    str(path / 'test.cpp'), '-o',
                    str(path / 'test')
                ],
                               check=True)
                subprocess.run([str(path / 'test')], check=True)

    def test_delta_and_live_routing(self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        self.assertIn(
            '#if RME_HAS_INDX()\n  rme_tune_field("pellet_ejection_entry_only", config_store().indx_entry_only_pellet_ejection.get() ? 1 : 0);\n  #endif',
            source)
        # All recognized RME frames bypass FIFO, including this guarded setter.
        self.assertIn('|| handle_remote_tune_service(command)', source)
        self.assertIn('if (handle_remote_service_frame(command))', source)
        code = r'''
#include "src/common/rme_tune_delta.hpp"
#include <cassert>
int main(){
 rme_tune::Delta d;
 assert(!d.begin({})); assert(d.changed("pellet_ejection_entry_only",0));
 assert(d.begin(d.sequence())); assert(!d.changed("pellet_ejection_entry_only",0));
 assert(d.begin(d.sequence())); assert(d.changed("pellet_ejection_entry_only",1));
 assert(d.begin(d.sequence())); assert(d.changed("pellet_ejection_entry_only",0));
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            (path / 'test.cpp').write_text(code)
            subprocess.run([
                'c++', '-std=c++20', '-I' + str(ROOT),
                str(path / 'test.cpp'), '-o',
                str(path / 'test')
            ],
                           check=True)
            subprocess.run([str(path / 'test')], check=True)

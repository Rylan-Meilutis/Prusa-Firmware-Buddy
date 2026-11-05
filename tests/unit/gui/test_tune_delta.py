"""Native bounded tune-cache and recovery protocol regression."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class TuneDelta(unittest.TestCase):

    def test_integer_serializer_preserves_widths_and_sentinels(self):
        source = (ROOT / 'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text()
        serializer = function(source,
                              'template<class T> static void rme_tune_field(')
        code = r'''
#include <rme_tune_delta.hpp>
#include <cassert>
#include <sstream>
rme_tune::Delta tune_delta;
std::ostringstream serial;
#define SERIAL_ECHO(x) (serial << (x))
#define SERIAL_CHAR(x) (serial << (x))
''' + serializer + r'''
int main() {
 auto report=[] {
  rme_tune_field("fan",uint8_t(255));
  rme_tune_field("speed",uint16_t(65535));
  rme_tune_field("host_percent",int16_t(-1));
  rme_tune_field("progress_remaining",UINT32_MAX);
 };
 tune_delta.begin(std::nullopt); report();
 assert(serial.str()==" fan=255 speed=65535 host_percent=-1 progress_remaining=4294967295");
 serial.str(""); serial.clear();
 assert(tune_delta.begin(tune_delta.sequence())); report();
 assert(serial.str().empty());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'serializer.cpp'
            cpp.write_text(code)
            binary = Path(directory) / 'serializer'
            subprocess.run([
                'c++', '-std=c++23', '-I',
                str(ROOT / 'src/common'),
                str(cpp), '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

    def test_full_delta_loss_reconnect_and_unknown_sensors(self):
        code = r'''
#include <rme_tune_delta.hpp>
#include <cassert>
#include <limits>
int main() {
 rme_tune::Delta d;
 static_assert(sizeof(d)<512);
 assert(!d.begin(std::nullopt));
 assert(d.changed("fan",255)); assert(d.changed("progress_remaining",4294967295LL));
 assert(d.changed("toolmap",0x1abcdef01LL)); assert(d.changed("head_mcu",6000));
 auto n=d.sequence(); assert(d.begin(n));
 assert(!d.changed("fan",255)); assert(!d.changed("progress_remaining",4294967295LL));
 assert(!d.changed("toolmap",0x1abcdef01LL));
 assert(d.changed("head_mcu",std::numeric_limits<int64_t>::min()));
 // Dropped reply: the next since token requests a full recovery snapshot.
 assert(!d.begin(n)); assert(d.changed("fan",255));
 n=d.sequence(); d.reset(); assert(!d.begin(n)); assert(d.changed("fan",255));
 // Full requests are always full, even with unchanged values.
 assert(!d.begin(std::nullopt)); assert(d.changed("fan",255));
 n=d.sequence(); assert(d.begin(n)); assert(d.changed("fan",0));
 assert(d.changed("new_field",1)); // future fields cannot disappear silently
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'test.cpp'
            cpp.write_text(code)
            binary = Path(directory) / 'test'
            subprocess.run([
                'c++', '-std=c++23', '-I',
                str(ROOT / 'src/common'),
                str(cpp), '-o',
                str(binary)
            ],
                           check=True)
            subprocess.run([str(binary)], check=True)

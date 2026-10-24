"""Run the production idle-activity gate for read-only serial diagnostics."""
import unittest
import test_serial_pause_recovery as fixture


class PassiveDiagnostics(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_read_only_queries_neither_wake_lights_nor_touch_the_activity_timer(
            self):
        source = (fixture.ROOT / "src/common/serial_printing.cpp").read_text()
        gate = fixture.function(
            source, "if (rme_light_mode::serial_commands_wake_lights(")
        self.run_cpp(r'''
#include <cassert>
#include <rme_passive_command.hpp>
bool printing=false;
int activity=0;
namespace marlin_server { bool serial_print_active() {return printing;} }
namespace rme_light_mode { bool serial_commands_wake_lights(bool printing) {return !printing;} }
namespace leds { struct SideStripHandler {
 static SideStripHandler &instance() {static SideStripHandler h; return h;}
 void activity_ping() {++activity;}
}; }
void receive(const char *command) {
''' + gate + r'''
}
int main() {
 for (const char *query : {"M976 I", "M976 I T1", "M976 I L0 S240", "N12 M976 I T2*33",
       "M105", "M114", "M115", "M119", "M122", "M503", "M865 Q", "@RME TUNE QUERY",
       "@RME SESSION KEEPALIVE", "@RME FIRMWARE RUNNING", "M979", "N13 M979 ; status", "", "; M976"}) {
  receive(query); assert(activity==0);
 }
 for (const char *change : {"G1 X20", "M104 S240", "M976", "M976 T1", "M976 W1",
       "M976 A 0:0:HIPS:250", "M976 T0 ; I", "M9760 I", "M976.1 I", "M122 I1", "M979 S1 C180", "M979 S0"}) {
  const int previous=activity; receive(change); assert(activity==previous+1);
 }
 printing=true;
 const int previous=activity; receive("G1 X20"); assert(activity==previous);
}
''')


if __name__ == "__main__":
    unittest.main()

"""Execute manual pause confirmations and empty-head resume guards."""
from pathlib import Path
import unittest
import test_serial_pause_recovery as fixture

ROOT = Path(__file__).resolve().parents[3]


class PauseConfirmation(unittest.TestCase):

    def test_manual_pause_defaults_to_no_without_delaying_backend_pauses(self):
        local = fixture.function(
            (ROOT / 'src/gui/screen_printing.cpp').read_text(),
            'void screen_printing_data_t::pauseAction()')
        serial = fixture.function(
            (ROOT / 'src/gui/screen_printing_serial.cpp').read_text(),
            'void screen_printing_serial_data_t::pauseAction()')
        code = r'''
#include <cassert>
#include <array>
#include <type_traits>
#define _(text) text
namespace std {template<class E> constexpr auto to_underlying(E e) {return static_cast<underlying_type_t<E>>(e);}}
enum class Response {No,Yes};
constexpr int Responses_YesNo=0;
Response answer=Response::No;
int dialogs=0, pauses=0, resumes=0;
Response MsgBoxWarning(const char *,int,int default_response) {
 assert(default_response==1); ++dialogs; return answer;
}
namespace printer_lock {bool value=false; bool locked() {return value;}}
namespace marlin_client {
 void print_pause() {++pauses;} void print_resume() {++resumes;}
 void gcode(const char *) {}
}
enum class BtnSocket {Middle};
enum class printing_state_t {PRINTING,SKIPPABLE_OPERATION,PAUSED,PRINTED,STOPPED};
struct {void request_skip() {}} skip;
auto &skippable_gcode() {return skip;}
bool post_print_filtration_active() {return false;}
void stop_post_print_filtration() {}
void screen_printing_reprint() {}
struct screen_printing_data_t {
 printing_state_t state=printing_state_t::PRINTING;
 struct Button {bool shadow=false; bool IsShadowed() {return shadow;}};
 std::array<Button,1> buttons{};
 auto GetState() {return state;}
 void change_print_state() {} void set_pause_icon_and_label() {}
 void pauseAction();
};
namespace marlin_server {enum class State {Finished,Aborted,Paused,Printing};}
struct {marlin_server::State print_state=marlin_server::State::Printing;} vars;
auto &marlin_vars() {return vars;}
struct screen_printing_serial_data_t {void pauseAction();};
''' + local + serial + r'''
int main() {
 screen_printing_data_t local; screen_printing_serial_data_t serial;
 local.pauseAction(); serial.pauseAction();
 assert(dialogs==2 && pauses==0);
 answer=Response::Yes; local.pauseAction(); serial.pauseAction();
 assert(dialogs==4 && pauses==2);
 local.state=printing_state_t::PAUSED;
 vars.print_state=marlin_server::State::Paused;
 local.pauseAction(); serial.pauseAction();
 assert(dialogs==4 && resumes==2);
 // Safety and host @pause commands use the backend, not UI confirmation.
 marlin_client::print_pause(); assert(dialogs==4 && pauses==3);
 printer_lock::value=true; local.pauseAction(); serial.pauseAction();
 assert(dialogs==4 && resumes==2);
}
'''
        fixture.SerialPauseRecovery.run_cpp(self, code)

    @unittest.skip(
        'Legacy targets use a permanently attached hotend, not the modern empty-head API'
    )
    def test_empty_head_resume_never_restores_e_through_invalid_tool(self):
        unpark = fixture.function(
            (ROOT / 'src/common/marlin_server.cpp').read_text(),
            'void unpark_head_ZE(void)')
        code = r'''
#include <cassert>
#include <optional>
#define HAS_PAUSE() 1
#define NOZZLE_PARK_Z_FEEDRATE 10
bool selected=false, homed=false;
int z_moves=0, e_moves=0;
struct Pos {float z=0,e=0;} destination,current_position;
struct {struct {Pos pos;} resume;} server;
struct PhysicalToolIndex {
 static std::optional<int> currently_selected_opt() {
  return selected ? std::optional<int>(0) : std::nullopt;
 }
};
bool all_axes_homed() {return homed;}
void prepare_internal_move_to_destination(float) {++z_moves;}
namespace buddy::standard_feedrates {
enum class Extruder {deretract}; float current_extruder(Extruder) {assert(selected); return 1;}
}
namespace mapi {void extruder_move(float,float) {assert(selected); ++e_moves;}}
''' + unpark + r'''
int main() {
 unpark_head_ZE(); assert(z_moves==0 && e_moves==0);
 homed=true; unpark_head_ZE(); assert(z_moves==1 && e_moves==0);
 selected=true; unpark_head_ZE(); assert(z_moves==2 && e_moves==1);
}
'''
        fixture.SerialPauseRecovery.run_cpp(self, code)


if __name__ == '__main__':
    unittest.main()

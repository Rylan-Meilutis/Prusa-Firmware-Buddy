"""Execute production serial preparation under ASan/UBSan without hardware."""
import unittest
import test_serial_pause_recovery as fixture


class SerialPreparation(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_new_review_replaces_only_terminal_print_results(self):
        source = (fixture.ROOT / "src/common/marlin_server.cpp").read_text()
        method = fixture.function(source, "bool dismiss_print_result()")
        self.run_cpp(r'''
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <serial_print_finalize_policy.hpp>
using marlin_server::State;
enum class ClientFSM { Printing, Serial_printing, Warning, LoadUnload, _count };
struct { State print_state = State::Finished; bool print_is_serial = true; } server;
bool locked = false;
namespace printer_lock { bool locked() { return ::locked; } }
struct {
    bool active[4] = {};
    bool is_active(ClientFSM type) { return active[static_cast<size_t>(type)]; }
} fsm_states;
int destroyed = 0, published = 0;
void fsm_destroy(ClientFSM type) { fsm_states.active[static_cast<size_t>(type)]=false; ++destroyed; }
void _server_update_vars() { ++published; }
''' + method + r'''
int main() {
    for (auto state : { State::Printing, State::Paused, State::SerialPrintInit,
                       State::Aborting_WaitIdle, State::Finishing_WaitIdle, State::Idle }) {
        server.print_state=state; fsm_states.active[1]=true;
        assert(!dismiss_print_result());
        assert(fsm_states.active[1] && destroyed==0);
    }
    for (auto state : { State::Finished, State::Aborted }) {
        server.print_state=state; server.print_is_serial=true;
        fsm_states.active[0]=fsm_states.active[1]=true;
        for (int dialog : {2,3}) {
            fsm_states.active[dialog]=true;
            assert(!dismiss_print_result());
            assert(fsm_states.active[0] && fsm_states.active[1]);
            fsm_states.active[dialog]=false;
        }
        locked=true; assert(!dismiss_print_result()); locked=false;
        assert(dismiss_print_result());
        assert(server.print_state==State::Idle && !server.print_is_serial);
        assert(!fsm_states.active[0] && !fsm_states.active[1]);
        assert(!dismiss_print_result());
    }
    assert(destroyed==4 && published==2);
}
''')
        queue = (fixture.ROOT /
                 "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        begin = queue.split('if (rme_protocol::action_is(action, "BEGIN")) {',
                            1)[1]
        self.assertLess(begin.index("dismiss_print_result()"),
                        begin.index("const bool has_fsm"))

    def test_validation_is_job_scoped_and_cannot_timeout_into_printing(self):
        source = (fixture.ROOT / "src/common/marlin_server.cpp").read_text()
        methods = "\n".join(
            fixture.function(source, signature)
            for signature in ("bool serial_print_preparing()",
                              "bool serial_print_prepare(",
                              "bool serial_print_release_preparation(",
                              "bool serial_print_fifo_held()"))
        loop = source.split(
            "    case State::PrintInit:\n    case State::SerialPrintInit:\n#if HAS_SERIAL_PRINT()",
            1)[1]
        wait = fixture.function(loop, "if (serial_print_preparing()) {")
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
enum class State { Idle, SerialPrintInit, Printing, Finished, Aborted, Aborting_Preview };
enum class ClientFSM { Serial_printing };
enum class PhasesSerialPrinting { active };
struct { State print_state = State::Idle; bool print_is_serial = false; } server;
bool lease = true, locked = false, processing = false, screen = false;
int timer_starts = 0, print_starts = 0, creates = 0, host_cancels = 0;
namespace serial_remote_control { bool session_active() {return lease;} }
namespace printer_lock { bool locked() {return ::locked;} }
bool is_processing() {return processing;}
struct { bool is_active(ClientFSM) {return screen;} } fsm_states;
void fsm_create(PhasesSerialPrinting) {screen=true; ++creates;}
static uint32_t serial_preparation_id = 0;
void serial_print_start() {server.print_state = State::SerialPrintInit; ++print_starts;}
void print_abort() {serial_preparation_id = 0; ++host_cancels; server.print_state = State::Aborting_Preview;}
bool serial_print_fifo_held(bool, State) {return false;}
''' + methods + r'''
void tick() {
    switch (server.print_state) {
    case State::SerialPrintInit:
''' + wait + r'''
        ++timer_starts;
        server.print_state = State::Printing;
        break;
    default: break;
    }
}
int main() {
    assert(!serial_print_prepare(0));
    lease=false; assert(!serial_print_prepare(1)); lease=true;
    locked=true; assert(!serial_print_prepare(1)); locked=false;
    processing=true; assert(!serial_print_prepare(1)); processing=false;
    assert(serial_print_prepare(42));
    assert(serial_print_prepare(42)); // Idempotent retry, not another job.
    assert(!serial_print_prepare(43));
    assert(print_starts==1 && timer_starts==0 && serial_print_fifo_held());
    for (int seconds=0; seconds<180; ++seconds) tick();
    assert(screen && creates==1 && timer_starts==0 && serial_print_preparing());
    assert(!serial_print_release_preparation(41, false));
    assert(serial_print_release_preparation(42, false));
    tick(); assert(timer_starts==1 && server.print_state==State::Printing && !serial_print_fifo_held());
    assert(!serial_print_prepare(43)); // Cannot hijack an active print.
    assert(!serial_print_release_preparation(42, false));
    server.print_state=State::Idle; screen=false;
    assert(serial_print_prepare(43)); tick(); lease=false; tick();
    assert(server.print_state==State::Aborting_Preview && !serial_print_preparing());
    assert(timer_starts==1 && host_cancels==1); // Lost host never starts motion.
    lease=true; server.print_state=State::Idle;
    assert(serial_print_prepare(44));
    assert(serial_print_release_preparation(44, true));
    assert(host_cancels==2 && timer_starts==1);
}
''')


if __name__ == "__main__":
    unittest.main()

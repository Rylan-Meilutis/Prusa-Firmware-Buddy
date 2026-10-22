"""Execute the production RME review service and LCD tick under ASan/UBSan."""
import unittest
import test_serial_pause_recovery as fixture


class ToolReview(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_job_identity_lcd_web_commit_and_abort_faults(self):
        source = (fixture.ROOT /
                  "lib/Marlin/Marlin/src/gcode/queue.cpp").read_text()
        methods = "\n".join(
            fixture.function(source, signature)
            for signature in ("static bool remote_review_in_front()",
                              "static uint32_t remote_review_revision()",
                              "static void report_remote_review()",
                              "static void remote_review_close(",
                              "static void remote_review_tick()",
                              "static bool handle_remote_review_service("))
        self.run_cpp(r'''
#include <cassert>
#include <array>
#include <sstream>
#include <utility>
#include <optional>
#include <string_view>
#include <rme_protocol_parser.hpp>
#include <rme_tool_review.hpp>
namespace std { template<class E> constexpr auto to_underlying(E e) { return static_cast<int>(e); } }
std::ostringstream serial;
#define SERIAL_ECHOPGM(x) serial << x
#define SERIAL_ECHOLNPGM(x) serial << x << '\n'
#define SERIAL_ECHO(x) serial << x
#define SERIAL_CHAR(x) serial << x
#define SERIAL_EOL() serial << '\n'
enum class ClientFSM { PrintPreview, Filament };
enum class PhasesPrintPreview { tools_mapping };
enum class Response { _none, Print, Abort };
bool session=true, idle=true, printing=false, locked=false;
int creates=0, destroys=0, metadata=0;
Response response=Response::_none;
struct Data { int GetPhase() const {return 0;} };
struct Top { ClientFSM fsm_type=ClientFSM::PrintPreview; Data data; };
std::optional<Top> top;
namespace fsm { struct States { auto get_top() const {return top;} }; }
struct Vars { template<class F> auto peek_fsm_states(F f) {return f(fsm::States{});} } vars;
Vars &marlin_vars() {return vars;}
namespace printer_lock { bool locked() {return ::locked;} }
namespace serial_remote_control { bool session_active() {return session;} }
namespace marlin_server {
bool printer_idle() {return idle;}
bool serial_print_active() {return printing;}
void fsm_create(PhasesPrintPreview,std::array<uint8_t,4>) {assert(!top); ++creates; top=Top{};}
void fsm_destroy(ClientFSM) {++destroys; top.reset();}
Response get_response_from_phase(PhasesPrintPreview) {return std::exchange(response,Response::_none);}
}
constexpr size_t filament_name_buffer_size=8;
struct GcodeToolIndex {static constexpr int count=3; static int from_raw(int i) {return i;} };
struct VirtualToolIndex {static constexpr int count=3;};
struct GCodeInfo {
 static GCodeInfo &getInstance() {static GCodeInfo info; return info;}
 void reset_info() {metadata=0;}
 void set_serial_review_tool(int, std::string_view) {++metadata;}
};
struct {bool enabled=true; std::array<uint8_t,3> map{0,1,2};
 bool is_enabled() {return enabled;}
 uint8_t to_virtual(int i,bool) {return map[i];}
 void set_mapping(int i,uint8_t t) {map[i]=t;}
 void set_enable(bool e) {enabled=e;}
} tool_mapper;
struct Join {uint8_t spool_1,spool_2;};
struct {uint8_t count=0; std::array<Join,2> joins{};
 uint8_t get_num_joins() {return count;}
 Join get_join_nr(int i) {return joins[i];}
 void reset() {count=0;}
} spool_join;
auto remote_number(std::string_view c,std::string_view k) {return rme_protocol::signed_number(c,k);}
static rme_tool_review::State remote_review;
''' + methods + r'''
void command(std::string_view c) {assert(handle_remote_review_service(c));}
void begin(unsigned id) {
 command("@RME REVIEW BEGIN id="+std::to_string(id)+" mapping=0,1,2 enabled=1");
 command("@RME REVIEW TOOL id="+std::to_string(id)+" logical=0 material=ASA");
 command("@RME REVIEW SHOW id="+std::to_string(id));
}
void commit(unsigned id,uint32_t rev) {
 command("@RME REVIEW COMMIT id="+std::to_string(id)+" revision="+std::to_string(rev));
}
int main() {
 using enum rme_tool_review::Phase;
 command("@RME REVIEW BEGIN id=0"); assert(remote_review.phase==rme_tool_review::Phase::idle);
 command("@RME REVIEW BEGIN id=1 mapping=0,0,2 enabled=1"); assert(!remote_review.active());
 locked=true; begin(1); assert(!remote_review.active()); locked=false;
 printing=true; begin(1); assert(!remote_review.active()); printing=false;
 top=Top{ClientFSM::Filament}; begin(1); assert(!remote_review.active()); top.reset();
 command("@RME REVIEW BEGIN id=1"); command("@RME REVIEW SHOW id=1");
 assert(remote_review.phase==loading && !top); // Missing metadata cannot open UI.
 begin(1); assert(remote_review.phase==editing && metadata==1 && creates==1);
 begin(1); assert(creates==1 && metadata==1); // Retry cannot reset a live screen.
 begin(2); assert(remote_review.id==1 && creates==1);
 commit(1,0); assert(remote_review.phase==editing); // No confirmation.
 command("@RME REVIEW SUBMIT id=2"); assert(remote_review.phase==editing);
 // A native loading dialog must finish before web/LCD can confirm mapping.
 top=Top{ClientFSM::Filament}; response=Response::Print; remote_review_tick();
 command("@RME REVIEW SUBMIT id=1"); assert(remote_review.phase==editing);
 top=Top{}; spool_join.count=1; spool_join.joins[0]={0,2};
 remote_review_tick(); assert(remote_review.phase==submitted && !top && destroys==1);
 auto rev=remote_review.revision;
 commit(2,rev); commit(1,rev+1); assert(remote_review.phase==submitted);
 tool_mapper.map[0]=2; commit(1,rev); assert(remote_review.phase==submitted);
 assert(remote_review.revision!=rev); rev=remote_review.revision;
 commit(1,rev); assert(remote_review.phase==complete);
 command("@RME REVIEW SHOW id=1"); command("@RME REVIEW BEGIN id=1"); commit(1,rev);
 assert(remote_review.phase==complete && creates==1); // Late events stay terminal.
 begin(2);
 spool_join.count=1; spool_join.joins[0]={0,2};
 command("@RME REVIEW BEGIN id=2 mapping=0,1,2 enabled=1");
 assert(spool_join.count==1); // Retrying BEGIN cannot erase live joins.
 command("@RME REVIEW SUBMIT id=2"); assert(remote_review.phase==submitted);
 command("@RME REVIEW REJECT id=2"); assert(remote_review.phase==editing && top);
 response=Response::Abort; remote_review_tick(); assert(remote_review.phase==cancelled && !top);
 begin(3); session=false; remote_review_tick(); assert(remote_review.phase==cancelled && !top);
 session=true; begin(4); ::idle=false; remote_review_tick(); assert(remote_review.phase==cancelled && !top);
 ::idle=true; begin(5); command("@RME REVIEW CANCEL id=5");
 assert(remote_review.phase==cancelled && !top);
 assert(sizeof(remote_review)<=16); // Keep the INDX SRAM floor intact.
}
''')


if __name__ == "__main__":
    unittest.main()

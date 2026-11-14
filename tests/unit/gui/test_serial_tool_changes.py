"""Execute the production count receiver and LCD item selection under sanitizers."""
import unittest
import test_serial_pause_recovery as fixture


class SerialToolChanges(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_wire_counts_expiry_reset_and_fourth_lcd_item(self):
        source = (fixture.ROOT / 'src/common/serial_printing.cpp').read_text()
        methods = '\n'.join(
            fixture.function(source, signature)
            for signature in ('void SerialPrinting::set_host_tool_changes(',
                              'void SerialPrinting::clear_host_tool_changes(',
                              'bool SerialPrinting::host_tool_changes('))
        receiver = fixture.function(
            (fixture.ROOT /
             'lib/Marlin/Marlin/src/gcode/queue.cpp').read_text(),
            'static bool handle_remote_progress_service(')
        lcd = (fixture.ROOT / 'src/gui/screen_printing_serial.cpp').read_text()
        available = fixture.function(
            lcd, 'bool screen_printing_serial_data_t::time_item_available(')
        display = fixture.function(lcd,
                                   'case TimeItem::completed_tool_changes: {')
        header = (fixture.ROOT /
                  'src/gui/screen_printing_serial.hpp').read_text()
        items = fixture.function(header, 'enum class TimeItem') + ';'
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <optional>
#include <string_view>
#include <sstream>
#include <array>
#include <cstdio>
#include <cstring>
#include <rme_protocol_parser.hpp>
#include <rme_tool_change_progress.hpp>
#define PRINTER_IS_PRUSA_MINI() 0
std::ostringstream serial;
#define SERIAL_ECHOLNPGM(x) serial << x << '\n'
#define SERIAL_ECHOPGM(x) serial << x
#define SERIAL_ECHO(x) serial << x
#define SERIAL_ECHOLN(x) serial << x << '\n'
uint32_t clock_ms = 0;
uint32_t ticks_ms() {return clock_ms;}
bool lease = true, job = true;
namespace buddy {struct InterruptDisabler {};}
namespace serial_remote_control {bool session_active() {return lease;}}
namespace marlin_server {
constexpr uint32_t TIME_TO_END_INVALID = 0xffffffff;
bool serial_print_active() {return job;}
}
struct Value {uint32_t get() const {return 100;}};
struct {Value time_to_end, time_to_pause;} vars;
auto &marlin_vars() {return vars;}
static rme_tool_change_progress::State tool_change_progress;
struct SerialPrinting {
    static void set_host_tool_changes(uint32_t, uint32_t);
    static void clear_host_tool_changes();
    static bool host_tool_changes(uint32_t &, uint32_t &, uint32_t);
    static void set_rme_progress(uint32_t, uint32_t, bool) {}
};
size_t strlcpy(char *target, const char *source, size_t size) {
    auto length=strlen(source);
    if(size) {auto count=length<size-1?length:size-1;memcpy(target,source,count);target[count]=0;}
    return length;
}
#define _(text) text
''' + methods + receiver + r'''
struct screen_printing_serial_data_t {
''' + items + r'''
    bool time_item_available(TimeItem) const;
    std::array<char,32> w_etime_value_buffer{};
    struct {const char *text=nullptr; void SetText(const char *value) {text=value;}} w_etime_label;
    void render_count() {
        switch(TimeItem::completed_tool_changes) {
''' + display + r'''
        default: break;
        }
    }
};
''' + available + r'''
int main() {
    using Item = screen_printing_serial_data_t::TimeItem;
    static_assert(static_cast<int>(Item::completed_tool_changes) == 3);
    screen_printing_serial_data_t lcd;
    uint32_t left = 999, total = 999;
    assert(!lcd.time_item_available(Item::completed_tool_changes));
    auto send = [](std::string_view fields) {
        serial.str(""); serial.clear();
        assert(handle_remote_progress_service(std::string("@RME PROGRESS SET percent=10 remaining=100 paused=0 ") + std::string(fields)));
    };
    send("tools_remaining=3 tools_total=9");
    assert(serial.str().find("accepted=1") != std::string::npos);
    assert(SerialPrinting::host_tool_changes(left, total, clock_ms) && left == 3 && total == 9);
    assert(lcd.time_item_available(Item::completed_tool_changes));
    for (auto malformed : {"tools_remaining=10 tools_total=9", "tools_remaining=-1 tools_total=9",
            "tools_remaining=1", "tools_total=2", "tools_remaining=unknown tools_total=9",
            "tools_remaining=0 tools_total=200001"}) {
        send(malformed);
        assert(serial.str().find("invalid_argument") != std::string::npos);
        assert(SerialPrinting::host_tool_changes(left, total, clock_ms) && left == 3 && total == 9);
    }
    lease=false; send("tools_remaining=1 tools_total=9");
    assert(serial.str().find("no_serial_job") != std::string::npos);
    assert(!lcd.time_item_available(Item::completed_tool_changes)); lease=true;
    job=false; send("tools_remaining=1 tools_total=9");
    assert(serial.str().find("no_serial_job") != std::string::npos); job=true;
    clock_ms=60001; assert(!lcd.time_item_available(Item::completed_tool_changes));
    send("tools_remaining=0 tools_total=9");
    assert(lcd.time_item_available(Item::completed_tool_changes));
    lcd.render_count(); assert(!strcmp(lcd.w_etime_value_buffer.data(),"9 / 9"));
    assert(!strcmp(lcd.w_etime_label.text,"Tool changes done"));
    send("tools_remaining=0 tools_total=0");
    assert(SerialPrinting::host_tool_changes(left, total, clock_ms) && left==0 && total==0);
    assert(lcd.time_item_available(Item::completed_tool_changes));
    lcd.render_count(); assert(!strcmp(lcd.w_etime_value_buffer.data(),"0 / 0"));
    for (auto remaining : {54u,53u,27u,0u}) {
        send("tools_remaining="+std::to_string(remaining)+" tools_total=54");
        lcd.render_count();
        assert(lcd.w_etime_value_buffer.data()==std::to_string(54-remaining)+" / 54");
        assert(lcd.time_item_available(Item::completed_tool_changes));
        // Only presentation counts up: the compatible wire value stays remaining.
        assert(SerialPrinting::host_tool_changes(left,total,clock_ms) && left==remaining && total==54);
    }
    send("tools_remaining=200000 tools_total=200000");
    assert(lcd.time_item_available(Item::completed_tool_changes));
    lcd.render_count(); assert(!strcmp(lcd.w_etime_value_buffer.data(),"0 / 200000"));
    send("tools_remaining=unknown tools_total=unknown");
    assert(!SerialPrinting::host_tool_changes(left, total, clock_ms));
    lcd.render_count(); assert(!strcmp(lcd.w_etime_value_buffer.data(),"--"));
    // Normal old-host progress packets do not invent a tool-change count.
    send(""); assert(!lcd.time_item_available(Item::completed_tool_changes));
    clock_ms=0xfffffff0; send("tools_remaining=1 tools_total=2");
    clock_ms=32; assert(lcd.time_item_available(Item::completed_tool_changes));
    SerialPrinting::clear_host_tool_changes();
    assert(!lcd.time_item_available(Item::completed_tool_changes));
}
''')


if __name__ == '__main__':
    unittest.main()

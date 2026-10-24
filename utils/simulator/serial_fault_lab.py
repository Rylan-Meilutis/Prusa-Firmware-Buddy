"""Host-in-loop serial fault lab; no hardware ports or recovery motion.

Executes extracted production firmware progress/session/timeout functions in
an ASan/UBSan native process, with the real RME plugin and OctoPrint file reader.
This is NOT a CORE One INDX hardware or power-panic motion emulator.
"""
import argparse
import hashlib
import json
import logging
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def native_source():
    serial = (ROOT / 'src/common/serial_printing.cpp').read_text()
    remote = (ROOT / 'src/gui/serial_remote_control.cpp').read_text()
    server = (ROOT / 'src/common/marlin_server.cpp').read_text()
    functions = '\n'.join(
        function(serial, signature)
        for signature in ('bool SerialPrinting::has_serial_timeouted()',
                          'void SerialPrinting::print_loop()',
                          'void SerialPrinting::reset_host_progress()',
                          'bool SerialPrinting::host_progress_percent(',
                          'bool SerialPrinting::host_time_to_end(',
                          'void SerialPrinting::set_rme_progress('))
    return r'''
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <rme_host_progress.hpp>
#include <marlin_server_types/marlin_server_state.h>
#define HAS_TOOL_CRASH_RECOVERY() 0
uint32_t clock_ms=1000;
uint32_t ticks_ms() {return clock_ms;}
int32_t ticks_diff(uint32_t a,uint32_t b) {return int32_t(a-b);}
bool queued=false,moving=false;
struct GCodeQueue {static bool has_commands_queued() {return queued;}};
struct {bool processing() {return moving;}} planner;
namespace marlin_server {
struct {State print_state=State::Printing;} server;
void capture_print_duration() {}
void serial_print_finalize();
}
''' + 'namespace marlin_server {\n' + function(
        server, 'void serial_print_finalize(void)') + r'''
}
rme_host_progress::State rme_progress;
struct SerialPrinting {
static inline uint32_t last_serial_indicator_ms=1000,last_host_progress_ms=0,
last_host_time_to_end_ms=0,last_host_time_to_end_s=0;
static inline uint8_t last_host_progress_percent=0;
static uint32_t serial_printing_screen_timeout_ms() {return 30000;}
static bool has_serial_timeouted(); static void print_loop();
static void reset_host_progress();
static bool host_progress_percent(uint8_t&,uint32_t);
static bool host_time_to_end(uint32_t&,uint32_t);
static void set_rme_progress(uint8_t,uint32_t,bool);
};
''' + functions + r'''
namespace serial_remote_control {
std::atomic<bool> protocol_session_active{true};
std::atomic<uint32_t> protocol_session_activity_ms{1000};
constexpr uint32_t protocol_session_timeout_ms=30000;
void close_session() {protocol_session_active.store(false);}
''' + function(remote, 'bool session_active()') + '\n' + function(
            remote, 'void keepalive_session()') + r'''
}
int main() {
 std::string line;
 while(std::getline(std::cin,line)) {
  std::istringstream input(line); std::string command; input>>command;
  if(command=="reset") {
   clock_ms=1000; queued=moving=false;
   marlin_server::server.print_state=marlin_server::State::Printing;
   SerialPrinting::last_serial_indicator_ms=1000; SerialPrinting::reset_host_progress();
   serial_remote_control::protocol_session_active=true;
   serial_remote_control::protocol_session_activity_ms=1000;
  } else if(command=="tick") {
   uint32_t delta; input>>delta; clock_ms+=delta;
   if(marlin_server::server.print_state==marlin_server::State::Printing) SerialPrinting::print_loop();
  } else if(command=="host") {
   unsigned percent,seconds,paused; input>>percent>>seconds>>paused;
   SerialPrinting::set_rme_progress(percent,seconds,paused);
  } else if(command=="activity") SerialPrinting::last_serial_indicator_ms=clock_ms;
  else if(command=="keepalive") serial_remote_control::keepalive_session();
  else if(command=="open") {
   serial_remote_control::protocol_session_active=true;
   serial_remote_control::protocol_session_activity_ms=clock_ms;
  } else if(command=="busy") {int value;input>>value;queued=value;}
  uint8_t percent=0;uint32_t seconds=0;
  bool valid=SerialPrinting::host_progress_percent(percent,clock_ms);
  bool eta=SerialPrinting::host_time_to_end(seconds,clock_ms);
  std::cout<<"{\"ms\":"<<clock_ms<<",\"session\":"
   <<(serial_remote_control::session_active()?"true":"false")
   <<",\"host_valid\":"<<(valid?"true":"false")
   <<",\"display_percent\":"<<(valid?int(percent):88)
   <<",\"remaining\":"<<(eta?seconds:1200)
   <<",\"finishing\":"<<(marlin_server::server.print_state==marlin_server::State::Finishing_WaitIdle?"true":"false")
   <<"}"<<std::endl;
 }
}
'''


class Firmware:

    def __init__(self, directory):
        directory = Path(directory)
        directory.mkdir(parents=True, exist_ok=True)
        options = directory / 'option'
        options.mkdir(exist_ok=True)
        for name in ('has_indx', 'has_mmu2', 'has_tool_crash_recovery',
                     'has_tool_mapping'):
            (options / (name + '.h')).write_text('#define ' + name.upper() +
                                                 '() 0\n')
        source = directory / 'firmware_fault_lab.cpp'
        source.write_text(native_source())
        binary = directory / 'firmware_fault_lab'
        subprocess.run([
            'c++', '-std=c++20', '-fsanitize=address,undefined', '-fno-pie',
            '-no-pie', '-I' + str(directory), '-I' + str(ROOT / 'src/common'),
            str(source), '-o',
            str(binary)
        ],
                       check=True)
        self.process = subprocess.Popen([str(binary)],
                                        stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE,
                                        text=True,
                                        env=dict(
                                            os.environ,
                                            ASAN_OPTIONS='detect_leaks=0'))
        self.trace = []

    def command(self, command):
        self.process.stdin.write(command + '\n')
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError('Native firmware simulation exited')
        state = json.loads(line)
        self.trace.append(dict(command=command, **state))
        return state

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=5)
        self.process.stdout.close()
        if self.process.returncode:
            raise RuntimeError('Native firmware simulation failed')


def run(output_dir, plugin_source):
    sys.path.insert(0, str(plugin_source))
    from octoprint.util.comm import PrintingGcodeFileInformation
    from octoprint_rme_compatibility.plugin import RmeCompatibilityPlugin
    from octoprint_rme_compatibility.protocol import parse_line

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    firmware = Firmware(output_dir)
    report = dict(
        scope=
        'Production-backed host fault lab, not INDX hardware/motion emulation',
        scenarios=[])
    plugin = RmeCompatibilityPlugin()
    plugin._logger = logging.getLogger('rme-serial-fault-lab')
    plugin._schedule_publish = lambda: None
    plugin._state['machine']['host_progress'] = 1
    plugin._state['session']['active'] = True
    plugin._printer = SimpleNamespace(
        get_state_id=lambda: 'PRINTING',
        is_paused=lambda: False,
        get_current_data=lambda: dict(job=dict(file=dict(origin='local')),
                                      progress=dict(completion=93,
                                                    printTimeLeft=600)))
    delivered = []

    def send(command, trigger):
        fields = dict(part.split('=', 1) for part in command.split()[3:])
        state = firmware.command(
            'host {percent} {remaining} {paused}'.format(**fields))
        delivered.append(command)
        plugin._handle_record(
            parse_line(
                'RME_PROGRESS accepted=1 percent=93 remaining=600 paused=0'))
        return state

    plugin._send_priority_service = send
    try:
        firmware.command('reset')
        for now in (100, 120, 140, 160):
            if now != 100:
                firmware.command('activity')
                firmware.command('keepalive')
                firmware.command('tick 20000')
            with patch('octoprint_rme_compatibility.plugin.time.monotonic',
                       return_value=now):
                plugin._sync_host_progress()
            state = firmware.command('status')
            assert state['display_percent'] == 93 and not state['finishing']
        report['scenarios'].append(
            dict(name='healthy host/slicer conflict',
                 host=93,
                 slicer=88,
                 firmware=state['display_percent'],
                 progress_frames=len(delivered),
                 result='No mismatch while host lease is fresh'))

        firmware.command('reset')
        firmware.command('host 93 600 0')
        for _ in range(7):
            firmware.command(
                'activity')  # Streamed M73/normal job traffic continues.
            firmware.command('keepalive')
            state = firmware.command('tick 10000')
        assert state['display_percent'] == 88 and state[
            'session'] and not state['finishing']
        restored = firmware.command('host 93 530 0')
        assert restored['display_percent'] == 93
        report['scenarios'].append(
            dict(
                name='drop host progress but keep other traffic',
                divergent=state,
                restored=restored,
                result=
                'Reproduces 93 host / 88 firmware after progress freshness expires'
            ))

        firmware.command('reset')
        before = firmware.command('tick 29999')
        expired = firmware.command('tick 2')
        reconnected = firmware.command('open')
        assert not before['finishing'] and expired[
            'finishing'] and not expired['session']
        assert reconnected['session'] and reconnected['finishing']
        report['scenarios'].append(
            dict(
                name='powered serial link loss and reconnect',
                before=before,
                expired=expired,
                reconnected=reconnected,
                result=
                'Current inactivity path finalizes rather than parking in a resumable disconnect pause'
            ))

        firmware.command('reset')
        firmware.command('busy 1')
        busy = firmware.command('tick 31000')
        firmware.command('busy 0')
        drained = firmware.command('tick 30001')
        assert not busy['finishing'] and drained['finishing']
        report['scenarios'].append(
            dict(
                name='disconnect with buffered motion',
                busy=busy,
                drained=drained,
                result=
                'Timeout refresh follows processing, not host delivery acknowledgements'
            ))

        path = output_dir / 'checkpoint_fixture.gcode'
        path.write_text(
            '; UTF-8: båat\r\nG90\r\nM83\r\nG1 X1 E1\r\nG1 X2 E1\r\nG1 X3 E1\r\n',
            newline='')
        current = PrintingGcodeFileInformation(str(path))
        current.start()
        lines = [current.getNext() for _ in range(5)]
        executed_offset = lines[2][
            1]  # Last completed command; next two merely buffered.
        sent_offset = current.getFilepos()
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        checkpoint = dict(version=1,
                          file_sha256=digest,
                          executed_offset=executed_offset,
                          diagnostic_line=lines[2][2],
                          planner_snapshot='SIMULATED-NO-MOTION',
                          mapping={'0': 0},
                          cancelled_objects=[])
        checkpoint_path = output_dir / 'simulated_checkpoint.json'
        checkpoint_path.write_text(json.dumps(checkpoint, indent=2))
        current.close()
        # New OctoPrint file reader, modeling host-process restart.
        checkpoint = json.loads(checkpoint_path.read_text())
        assert checkpoint['file_sha256'] == hashlib.sha256(
            path.read_bytes()).hexdigest()
        resumed = PrintingGcodeFileInformation(str(path))
        resumed.start()
        resumed.seek(checkpoint['executed_offset'])
        next_line = resumed.getNext()[0]
        resumed.close()
        assert next_line == lines[3][0] and sent_offset > executed_offset
        report['scenarios'].append(
            dict(
                name='host restart checkpoint handshake prototype',
                executed_offset=executed_offset,
                sent_offset=sent_offset,
                correct_next=next_line,
                skipped_if_last_sent=lines[3][0],
                result=
                'Verified exact file reader seek; planner restore and INDX power-loss motion are NOT simulated'
            ))
        report['production_recovery_enabled'] = False
        report['trace'] = firmware.trace
        (output_dir / 'report.json').write_text(
            json.dumps(report, indent=2, ensure_ascii=False))
        print(
            json.dumps(
                {
                    key: value
                    for key, value in report.items() if key != 'trace'
                },
                indent=2))
    finally:
        firmware.close()
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--plugin-source',
                        type=Path,
                        default=ROOT.parent / 'OctoPrint-RME-Compatibility')
    args = parser.parse_args()
    run(args.output_dir, args.plugin_source)

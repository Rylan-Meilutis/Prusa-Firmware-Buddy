"""Run the actual chamber wait with mocked clock, telemetry and RME sink."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from test_serial_pause_recovery import function

ROOT = Path(__file__).resolve().parents[3]


class ChamberWaitReporting(unittest.TestCase):

    def test_production_wait_lifecycle_and_thermal_semantics(self):
        source = (
            ROOT /
            'src/marlin_stubs/feature/chamber/M141_M191.cpp').read_text()
        body = function(source, 'void PrusaGcodeSuite::M141_no_parser(')
        code = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace buddy {using Temperature=float; struct SafetyTimerBlocker {};}
struct M141Args {float target_temp;bool wait_for_heating=false,wait_for_cooling=false;};
namespace PrusaGcodeSuite {void M141_no_parser(const M141Args &);}
struct Event{std::string workflow,state,message;};
std::vector<Event> events;
struct SerialPrinting {
 static void notify_workflow(const char*w,const char*s,const char*m){events.push_back({w,s,m});}
 static void notify_progress(const char*w,const char*s,const char*,const char*m,int){events.push_back({w,s,m});}
};
uint32_t now=0;uint32_t ticks_ms(){return now;}
int32_t ticks_diff(uint32_t a,uint32_t b){return int32_t(a-b);}
enum class Response{Ok,Skip,None};
enum class WarningType{FailedToReachChamberTemperature};
int warning_type_phase(WarningType){return 0;}
bool warning=false,skip=false,draining=false;
int mode=0,idles=0;
namespace marlin_server {
 bool is_warning_active(WarningType){return warning;}
 void set_warning(WarningType){warning=true;}
 void clear_warning(WarningType){warning=false;}
 Response get_response_from_phase(int){return Response::Skip;}
}
struct SkippableGCode {struct Guard {bool is_skip_requested(){return skip;}};void request_skip(){skip=true;}} skippable;
auto &skippable_gcode(){return skippable;}
struct Planner {bool draining(){return ::draining;}} planner;
struct Chamber {
 bool supported=true,wait=false;float target=0;
 std::optional<float> current=20;
 struct Caps {bool supported;bool temperature_control(){return supported;}};
 Caps capabilities(){return {supported};}
 std::optional<float> set_target_temperature(std::optional<float> t){target=t.value_or(0);return t;}
 void set_heating_wait_active(bool active){wait=active;}
 std::optional<float> current_temperature(){return current;}
} chamber_instance;
auto &chamber(){return chamber_instance;}
namespace PrintStatusMessage {enum Type{waiting_for_chamber_temp};}
struct Progress {float current,target;};
struct PrintStatusMessageGuard {template<auto> void update(Progress){}};
#define SERIAL_ERROR_MSG(x) ((void)0)
void idle(bool){
 ++idles;now+=1000;
 if(mode==1){skip=true;return;}
 if(mode==2){draining=true;return;}
 if(mode==3){now+=1800000;return;}
 auto &c=chamber_instance;
 if(!c.current)c.current=20;
 else if(*c.current<c.target)*c.current+=10;
 else *c.current-=10;
 assert(idles<50);
}
''' + body + r'''
void reset(){events.clear();now=0;skip=draining=warning=false;mode=idles=0;chamber_instance=Chamber{};}
void terminal(const char *state){assert(events.front().state=="open");assert(events.back().state==state);assert(!chamber().wait);for(auto&e:events)assert(e.workflow=="chamber_temperature");}
int main(){
 reset();PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("closed");
 assert(idles==3);assert(events[1].message=="Waiting for chamber: 20/50 C");
 assert(events[events.size()-2].message=="Waiting for chamber: 50/50 C");
 reset();chamber().current=80;PrusaGcodeSuite::M141_no_parser({50,false,true});terminal("closed");assert(idles==3);
 reset();chamber().current=80;PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("closed");assert(idles==0);
 reset();chamber().current={};PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("closed");assert(events[1].message=="Waiting for chamber: unknown/50 C");assert(idles==4);
 reset();mode=1;PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("skipped");
 reset();mode=2;PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("cancelled");
 reset();mode=3;PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("skipped");assert(!warning);
 reset();chamber().supported=false;PrusaGcodeSuite::M141_no_parser({50,true,false});assert(events.empty()&&idles==0);
 reset();PrusaGcodeSuite::M141_no_parser({50,false,false});assert(events.empty()&&idles==0);
 reset();PrusaGcodeSuite::M141_no_parser({0,true,false});assert(events.empty()&&idles==0);
 reset();draining=true;PrusaGcodeSuite::M141_no_parser({50,true,false});terminal("cancelled");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            (path / 'test.cpp').write_text(code)
            subprocess.run([
                'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-variable', '-fsanitize=address,undefined',
                '-fno-pie', '-no-pie',
                str(path / 'test.cpp'), '-o',
                str(path / 'test')
            ],
                           check=True)
            subprocess.run([str(path / 'test')],
                           check=True,
                           env={
                               **os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'
                           })

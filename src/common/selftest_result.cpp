#include "selftest_result.hpp"
#include <option/has_switched_fan_test.h>
#include <selftest_result_evaluation.hpp>

TestResult SelftestResult::get_print_fan(uint8_t tool) const {
    return tools[tool]._printFan;
}

void SelftestResult::set_print_fan(uint8_t tool, TestResult result) {
    tools[tool]._printFan = result;
}

TestResult SelftestResult::get_heatbreak_fan(uint8_t tool) const {
    return tools[tool]._heatBreakFan;
}

void SelftestResult::set_heatbreak_fan(uint8_t tool, TestResult result) {
    tools[tool]._heatBreakFan = result;
}

TestResult SelftestResult::get_fans_switched(uint8_t tool) const {
    return tools[tool]._fansSwitched;
}

void SelftestResult::set_fans_switched(uint8_t tool, TestResult result) {
    tools[tool]._fansSwitched = result;
}

TestResult SelftestResult::evaluate_fans(uint8_t tool) const {
#if HAS_SWITCHED_FAN_TEST()
    return SelftestSnake::evaluate_results(tools[tool]._printFan, tools[tool]._heatBreakFan, tools[tool]._fansSwitched);
#else
    return SelftestSnake::evaluate_results(tools[tool]._printFan, tools[tool]._heatBreakFan);
#endif
}

bool SelftestResult::has_heatbreak_fan_passed(uint8_t tool) const {
    return tools[tool]._heatBreakFan == TestResult_Passed
#if HAS_SWITCHED_FAN_TEST()
        && tools[tool]._fansSwitched == TestResult_Passed
#endif /* HAS_SWITCHED_FAN_TEST */
        ;
}

TestResult SelftestResult::get_nozzle_heater(uint8_t tool) const {
    return tools[tool]._nozzle;
}

void SelftestResult::set_nozzle_heater(uint8_t tool, TestResult result) {
    tools[tool]._nozzle = result;
}

TestResult SelftestResult::get_loadcell(uint8_t tool) const {
    return tools[tool]._loadcell;
}

void SelftestResult::set_loadcell(uint8_t tool, TestResult result) {
    tools[tool]._loadcell = result;
}

TestResult SelftestResult::get_dock_offset(uint8_t tool) const {
    return tools[tool]._dockoffset;
}

void SelftestResult::set_dock_offset(uint8_t tool, TestResult result) {
    tools[tool]._dockoffset = result;
}

TestResult SelftestResult::get_tool_offset(uint8_t tool) const {
    return tools[tool]._tooloffset;
}

void SelftestResult::set_tool_offset(uint8_t tool, TestResult result) {
    tools[tool]._tooloffset = result;
}

TestResult SelftestResult::get_gearbox(uint8_t tool) const {
    return tools[tool]._gears;
}

void SelftestResult::set_gearbox(uint8_t tool, TestResult result) {
    tools[tool]._gears = result;
}

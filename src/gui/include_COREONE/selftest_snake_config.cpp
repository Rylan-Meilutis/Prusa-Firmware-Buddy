#include "selftest_snake_config.hpp"
#include <selftest_types.hpp>
#include <selftest_result_evaluation.hpp>
#include <config_store/store_instance.hpp>
#include <option/has_switched_fan_test.h>

#include <option/has_chamber_filtration_api.h>

#if HAS_PRECISE_HOMING_COREXY()
    #include <module/prusa/homing_corexy.hpp>
#endif

#include <option/has_chamber_api.h>
#if HAS_CHAMBER_API()
    #include <feature/chamber/chamber.hpp>
#endif

#include <option/has_xbuddy_extension.h>
#if HAS_XBUDDY_EXTENSION()
    #include <feature/xbuddy_extension/xbuddy_extension.hpp>
#endif

using namespace buddy;

namespace SelftestSnake {

#if HAS_CHAMBER_API() && HAS_XBUDDY_EXTENSION()
namespace {
TestResult evaluate_xbe_chamber_fans(const XBEFanTestResults &chamber_results) {
    const TestResult cooling_fans_result = evaluate_results(chamber_results.fans[0], chamber_results.fans[1]);
    const TestResult filtration_fan_result = chamber_results.fans[2];

    if (cooling_fans_result == TestResult_Failed || filtration_fan_result == TestResult_Failed) {
        return TestResult_Failed;
    }
    if (cooling_fans_result == TestResult_Passed || filtration_fan_result == TestResult_Passed) {
        return TestResult_Passed;
    }
    if (cooling_fans_result == TestResult_Skipped || filtration_fan_result == TestResult_Skipped) {
        return TestResult_Skipped;
    }
    return TestResult_Unknown;
}
} // namespace
#endif

TestResult get_test_result(Action action, [[maybe_unused]] Tool tool) {

    SelftestResult sr = config_store().selftest_result.get();

    switch (action) {
    case Action::Fans: {
        TestResult res = merge_hotends_evaluations(
            [&](int8_t e) {
                return evaluate_results(sr.tools[e].evaluate_fans());
            });
#if HAS_CHAMBER_API()
        switch (chamber().backend()) {
    #if HAS_XBUDDY_EXTENSION()
        case Chamber::Backend::xbuddy_extension: {
            const auto chamber_results = config_store().xbe_fan_test_results.get();
            res = evaluate_results(res, evaluate_xbe_chamber_fans(chamber_results));
            break;
        }
    #endif /* HAS_XBUDDY_EXTENSION() */
        case Chamber::Backend::none:
            break;
        }
#endif /* HAS_CHAMBER_API() */
        return res;
    }
    case Action::ZAlign:
        return evaluate_results(sr.zalign);
    case Action::YCheck:
        return evaluate_results(sr.yaxis);
    case Action::XCheck:
        return evaluate_results(sr.xaxis);
#if HAS_PRECISE_HOMING_COREXY()
    case Action::PreciseHoming:
        return corexy_home_is_calibrated() ? TestResult::TestResult_Passed : TestResult::TestResult_Unknown;
#endif
    case Action::Loadcell:
        return merge_hotends(tool, [&](const int8_t e) {
            return evaluate_results(sr.tools[e].loadcell);
        });
    case Action::ZCheck:
        return evaluate_results(sr.zaxis);
    case Action::Heaters:
        return evaluate_results(sr.bed, merge_hotends_evaluations([&](int8_t e) {
            return evaluate_results(sr.tools[e].nozzle);
        }));
    case Action::Gears:
        return merge_hotends(tool, [&](const int8_t e) {
            return evaluate_results(sr.tools[e].gears);
        });
    case Action::DoorSensor:
        return evaluate_results(config_store().selftest_result_door_sensor.get());
    case Action::FilamentSensorCalibration:
        return merge_hotends(tool, [&](const int8_t e) {
            return evaluate_results(sr.tools[e].fsensor);
        });
    case Action::_count:
        break;
    }
    return TestResult_Unknown;
}

ToolMask get_tool_mask([[maybe_unused]] Tool tool) {
    return ToolMask::AllTools;
}

uint64_t get_test_mask(Action action) {
    switch (action) {
    case Action::Fans:
    case Action::Gears:
    case Action::DoorSensor:
    case Action::FilamentSensorCalibration:
#if HAS_PRECISE_HOMING_COREXY()
    case Action::PreciseHoming:
#endif
        bsod("This should be gcode");
    case Action::YCheck:
        return stmYAxis;
    case Action::XCheck:
        return stmXAxis;
    case Action::ZCheck:
        return stmZAxis;
    case Action::Heaters:
        return stmHeaters;
    case Action::Loadcell:
        return stmLoadcell;
    case Action::ZAlign:
        return stmZcalib;
    case Action::_count:
        break;
    }
    assert(false);
    return stmNone;
}

} // namespace SelftestSnake

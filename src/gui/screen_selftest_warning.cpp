/// @file
#include <gui/screen_selftest_warning.hpp>

#include <option/has_selftest.h>
static_assert(HAS_SELFTEST());

#include <marlin_client.hpp>
#include <selftest_result_evaluation.hpp>

bool ScreenSelftestWarning::should_show() {
    return !is_selftest_successfully_completed();
}

ScreenSelftestWarning::ScreenSelftestWarning()
    : PseudoScreenCallback {
        [] {
            if (should_show()) {
                marlin_client::set_warning(WarningType::SelftestNotSuccessfullyCompleted);
            }
        },
    } {}

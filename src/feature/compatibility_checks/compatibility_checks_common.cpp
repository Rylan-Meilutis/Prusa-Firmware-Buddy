/// @file
#include "compatibility_checks_common.hpp"

#include <utils/overloaded_visitor.hpp>
#include <config_store/store_instance.hpp>
#include <window_msgbox.hpp>

namespace buddy::compatibility_checks {

HWCheckSeverity CheckMetadata::evaluate_severity() const {
    return match(
        severity, //
        [](HWCheckSeverity v) -> HWCheckSeverity { return v; }, //
        [](HWCheckType type) -> HWCheckSeverity { return config_store().visit_hw_check(type, [](auto &t) { return t.get(); }); } //
    );
}

void gui_incompatibility_error(const CheckMetadata &check, Response abort_response) {
    MsgBoxError(_(check.description), { abort_response });
}

[[nodiscard]] bool gui_confirm_incompatibility_default(const CheckMetadata &check, Response abort_response) {
    return MsgBoxWarning(_(check.description), { abort_response, Response::Ignore }) == Response::Ignore;
}
} // namespace buddy::compatibility_checks

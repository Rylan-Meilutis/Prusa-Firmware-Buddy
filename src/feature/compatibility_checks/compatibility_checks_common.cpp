/// @file
#include "compatibility_checks_common.hpp"

#include <utils/overloaded_visitor.hpp>
#include <config_store/store_instance.hpp>

namespace buddy::compatibility_checks {

HWCheckSeverity CheckMetadata::evaluate_severity() const {
    return match(
        severity, //
        [](HWCheckSeverity v) -> HWCheckSeverity { return v; }, //
        [](HWCheckType type) -> HWCheckSeverity { return config_store().visit_hw_check(type, [](auto &t) { return t.get(); }); } //
    );
}

} // namespace buddy::compatibility_checks

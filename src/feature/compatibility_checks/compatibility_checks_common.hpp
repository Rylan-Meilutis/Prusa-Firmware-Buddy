/// @file
#pragma once

#include <variant>
#include <inplace_function.hpp>

#include <common/hw_check.hpp>
#include <utils/storage/enum_bitset.hpp>

namespace buddy::compatibility_checks {

struct CheckMetadata {
    /// Severity if the check fails
    /// Can either be a hardcoded severity or a HWCheckType with user-configurable severity
    std::variant<HWCheckSeverity, HWCheckType> severity;

    HWCheckSeverity evaluate_severity() const;

    /// Translatable error message, WITHOUT a trailing '.'
    const char *title;

    /// Translatable long message. Can provide further information. Written in full sentences.
    const char *description;
};

template <typename Check_>
struct ChecksTraits {
    using Check = Check_;

    using Metadata = const EnumArray<Check, CheckMetadata, Check::_cnt>;
    static Metadata metadata;

    using Bitset = EnumBitset<Check, Check::_cnt>;

    /// @returns false if the iteration should stop
    using Visitor = stdext::inplace_function<bool(const CheckMetadata &)>;

    /// @returns false if the iteration stopped by visitor returning false
    static bool visit_set_bits(const Bitset &bitset, const Visitor &visitor) {
        for (uint8_t i = 0; i < std::to_underlying(Check::_cnt); i++) {
            if (bitset.test(i)) {
                if (!visitor(metadata[i])) {
                    return false;
                }
            }
        }

        return true;
    }
};

/// Curiously-recurring template for compatibility reports
template <typename Report>
struct CompatibilityReportBase {
    /// @returns (first) failed check of the highest severity
    /// @param check_filter_f only considers failed checks that match the filter
    auto highest_severity_failed_check_filtered(auto check_filter_f) const {
        struct {
            std::optional<typename Report::FailedCheck> check;
            HWCheckSeverity severity = HWCheckSeverity::Ignore;
        } result;

        static_cast<const Report *>(this)->visit_failed_checks([&](const Report::FailedCheck &check) {
            if (!check_filter_f(check)) {
                return true;
            }

            const auto severity = check.meta->evaluate_severity();
            if (!result.check.has_value() || result.severity < severity) {
                result = { check, severity };
            }

            return true;
        });

        return result.check;
    }

    auto highest_severity_failed_check() const {
        return highest_severity_failed_check_filtered([](const Report::FailedCheck &) -> bool { return true; });
    }

    /// Severity of the failures
    HWCheckSeverity failure_severity() const {
        const auto check = highest_severity_failed_check();
        if (!check) {
            return HWCheckSeverity::Ignore;
        }

        return check->meta->evaluate_severity();
    }
};

} // namespace buddy::compatibility_checks

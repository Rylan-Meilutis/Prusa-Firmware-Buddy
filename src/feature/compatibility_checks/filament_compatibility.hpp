/// @file
#pragma once

#include "compatibility_checks_common.hpp"

#include <tool_index.hpp>
#include <marlin_server_types/general_response.hpp>

struct FilamentTypeParameters;

namespace buddy::filament_compatibility {

using namespace buddy::compatibility_checks;

/// Compatibility checks not tied to a specific tool, but to the printer in general
enum class GeneralCheck : uint8_t {
    // FILLME

    _cnt,
};

/// Filament-Tool compatibility checks
enum class ToolCheck : uint8_t {

    /// Fails if the filament printing temperatures exceed temperatures supported by the tool
    tool_max_temp,

    /// Fails if the filament is abrasive and the nozzle is not hardened
    abrasive,

    _cnt,

};

// Note: Needs to be outside bcs forward declarations
struct CompatibilityReportGenerateArgs {
    // CAREFUL: Reference (because params are big)
    const FilamentTypeParameters &filament;

    std::variant<VirtualToolIndex, AllTools, NoTool> tools;
};

struct CompatibilityReport : public CompatibilityReportBase<CompatibilityReport> {
    ChecksTraits<ToolCheck>::Bitset failed_tool_checks;
    ChecksTraits<GeneralCheck>::Bitset failed_general_checks;

    struct FailedCheck {
        const CheckMetadata *meta;
    };

    /// Generates a compatibility record
    /// Consecutive generate() calls accumulate reported errors
    void generate_noclear(const CompatibilityReportGenerateArgs &args);

    /// @returns false if the iteration should stop
    using FailedCheckVisitor = stdext::inplace_function<bool(const FailedCheck &)>;

    /// @returns false if the iteration stopped by visitor returning false
    bool visit_failed_checks(const FailedCheckVisitor &visitor) const;

    /// Similar to gui_confirm_all_incompatibilities, but for a single specific failed check
    [[nodiscard]] static bool gui_confirm_incompatibility(const FailedCheck &check, Response abort_response);

    /// Accumulates fails from the @p other reports to this one
    void operator|=(const CompatibilityReport &other);
};

} // namespace buddy::filament_compatibility

/// @file
#include "filament_compatibility.hpp"

#include <common/aggregate_arity.hpp>
#include <tool/physical_tool.hpp>
#include <window_msgbox.hpp>

// Needed for ChecksTraits<GeneralCheck>::metadata
using namespace buddy::compatibility_checks;
using namespace buddy::filament_compatibility;

template <>
constinit const ChecksTraits<GeneralCheck>::Metadata ChecksTraits<GeneralCheck>::metadata {
    // FILLME
};

template <>
constinit const ChecksTraits<ToolCheck>::Metadata ChecksTraits<ToolCheck>::metadata {
    {
        ToolCheck::tool_max_temp,
        CheckMetadata {
            .severity = HWCheckSeverity::Abort,
            .title = N_("Tool not high-temperature"),
            .description = N_("Filament requires higher temperatures than what the tool can provide."),
        },
    },
    {
        ToolCheck::abrasive,
        CheckMetadata {
            .severity = HWCheckType::nozzle,
            .title = N_("Filament abrasive"),
            .description = N_("Filament is abrasive, but tool does not have hardened nozzle installed."),
        },
    },
};

namespace buddy::filament_compatibility {

void CompatibilityReport::generate(const FilamentTypeParameters &filament, std::variant<VirtualToolIndex, AllTools, NoTool> tools) {
    for (VirtualToolIndex vti : tool_index_iterator(tools).skip_all_disabled()) {
        PhysicalTool::for_index(vti.to_physical()).filament_compatibility_report(filament, *this);
    }
}

bool CompatibilityReport::visit_failed_checks(const FailedCheckVisitor &visitor) const {
    const auto v = [&](const CheckMetadata &meta) { return visitor(FailedCheck { .meta = &meta }); };

    if (!ChecksTraits<GeneralCheck>::visit_set_bits(failed_general_checks, v)) {
        return false;
    }

    if (!ChecksTraits<ToolCheck>::visit_set_bits(failed_tool_checks, v)) {
        return false;
    }

    return true;
}

void CompatibilityReport::operator|=(const CompatibilityReport &other) {
    static_assert(aggregate_arity<CompatibilityReport>() == 2);
    failed_general_checks |= other.failed_general_checks;
    failed_tool_checks |= other.failed_tool_checks;
}

[[nodiscard]] bool CompatibilityReport::gui_confirm_all_incompatibilities(Response abort_response) const {
    const auto highest_severity_failed_check = this->highest_severity_failed_check();
    if (auto &ch = highest_severity_failed_check; ch.has_value() && ch->meta->evaluate_severity() == HWCheckSeverity::Abort) {
        MsgBoxError(_(ch->meta->description), { abort_response });
        return false;
    }

    return visit_failed_checks([&](const FailedCheck &check) -> bool {
        // Don't even bother showing ignore level severities
        if (check.meta->evaluate_severity() == HWCheckSeverity::Ignore) {
            return true;
        }

        else {
            return MsgBoxWarning(_(check.meta->description), { abort_response, Response::Ignore }) == Response::Ignore;
        }
    });
}

} // namespace buddy::filament_compatibility

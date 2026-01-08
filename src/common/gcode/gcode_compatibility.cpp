#include "gcode_compatibility.hpp"

#include <config_store/store_instance.hpp>
#include <gcode_info.hpp>

#include <option/has_mmu2.h>
#if HAS_MMU2()
    #include <feature/prusa/MMU2/mmu2_mk4.h>
#endif

#include <option/has_tool_mapping.h>
#if HAS_TOOL_MAPPING()
    #include <tools_mapping.hpp>
#endif

#include <option/has_spool_join.h>
#if HAS_SPOOL_JOIN()
    #include <module/prusa/spool_join.hpp>
#endif

namespace buddy::gcode_compatibility {

template <>
constinit const ChecksTraits<GeneralCheck>::Metadata ChecksTraits<GeneralCheck>::metadata {
    {
        GeneralCheck::printer_model,
        CheckMetadata {
            .severity = HWCheckType::model,
            .title = N_("Incompatible printer model"),
            .description = N_("G-Code is sliced for a different printer and is not compatible."),
        },
    },
#if HAS_GCODE_COMPATIBILITY()
        {
            GeneralCheck::gcode_compatibility_mode,
            CheckMetadata {
                .severity = HWCheckType::gcode_compatibility,
                .title = N_("G-Code compatibility mode"),
                .description = N_("G-Code is sliced for a different, but compatible printer model."),
            },
        },
#endif
        {
            GeneralCheck::gcode_level,
            CheckMetadata {
                .severity = HWCheckType::gcode_level,
                .title = N_("G-Code version mismatch"),
                .description = nullptr, // Feel free to fill this in when you figure what this error means
            },
        },
        {
            GeneralCheck::minimum_fw_version,
            CheckMetadata {
                .severity = HWCheckType::firmware,
                .title = N_("Firmware update required"),
                .description = N_("G-Code requires features from a newer firmare version to function properly."),
            },
        },
        {
            GeneralCheck::input_shaper,
            CheckMetadata {
                .severity = HWCheckType::input_shaper,
                .title = N_("Not sliced for Input Shaping"),
                .description = N_("G-Code is not sliced with Input Shaping support. Slicing with IS significantly shortens printing time."),
            },
        },
#if HAS_MMU2()
        {
            GeneralCheck::mmu,
            CheckMetadata {
                .severity = HWCheckType::firmware,
                .title = N_("Sliced for MMU"),
                .description = N_("G-Code is sliced for MMU, but the MMU is not enabled on the printer. Cannot print."),
            },
        },
#endif
        {
            GeneralCheck::unsupported_features,
            CheckMetadata {
                .severity = HWCheckType::firmware,
                .title = N_("Unsupported features"),
                .description = N_("G-Code requires some features the printer does not have."),
            },
        },
};

template <>
constinit const ChecksTraits<VirtualToolCheck>::Metadata ChecksTraits<VirtualToolCheck>::metadata {
    {
        VirtualToolCheck::correct_tool,
        CheckMetadata {
            .severity = HWCheckSeverity::Abort,
            .title = N_("Wrong tool"),
            .description = N_("Tool is disabled or of a different type."),
        },
    },
    {
        VirtualToolCheck::nozzle_diameter,
        CheckMetadata {
            .severity = HWCheckType::nozzle,
            .title = N_("Wrong nozzle diameter"),
            .description = N_("G-Code is sliced for a different tool diameter."),
        },
    },
    {
        VirtualToolCheck::nozzle_hardened,
        CheckMetadata {
            .severity = HWCheckType::nozzle,
            .title = N_("Nozzle not hardened"),
            .description = N_("G-Code is sliced for a hardened nozzle. Non-hardened nozzles can get damaged due to material abrasivity."),
        },
    },
    {
        VirtualToolCheck::nozzle_high_flow,
        CheckMetadata {
            .severity = HWCheckType::nozzle,
            .title = N_("Nozzle not high-flow"),
            .description = N_("G-Code is sliced for a high-flow nozzle. Printing with standard nozzle can lead to underextrusion and extruder skipping."),
        },
    },
    {
        VirtualToolCheck::nozzle_not_high_flow,
        CheckMetadata {
            .severity = HWCheckType::nozzle,
            .title = N_("Nozzle high-flow mismatch"),
            .description = N_("G-Code is sliced for a non-high flow nozzle. High-flow nozzles require more purging, so printing a MMU print with a high-flow nozzle can result in a color creep."),
        },
    },
    {
        VirtualToolCheck::filament_loaded,
        CheckMetadata {
            .severity = HWCheckSeverity::Warning,
            .title = N_("Filament not loaded"),
            .description = N_("One of the used tools does not have a filament loaded. Filament sensors need to be disabled for the print."),
        },
    },
};

template <>
constinit const ChecksTraits<GCodeToolCheck>::Metadata ChecksTraits<GCodeToolCheck>::metadata {
    {
        GCodeToolCheck::tool_assigned,
        CheckMetadata {
            .severity = HWCheckSeverity::Abort,
            .title = N_("Unmapped tool"),
            .description = N_("G-Code tool is not mapped"),
        },
    },
};

HWCheckSeverity CheckMetadata::evaluate_severity() const {
    return match(
        severity, //
        [](HWCheckSeverity v) -> HWCheckSeverity { return v; }, //
        [](HWCheckType type) -> HWCheckSeverity { return config_store().visit_hw_check(type, [](auto &t) { return t.get(); }); } //
    );
}

bool CompatibilityReport::visit_failed_checks(const FailedCheckVisitor &visitor) const {
    {
        const auto v = [&](const CheckMetadata &meta) {
            return visitor(FailedCheck {
                .meta = &meta,
                .tool = NoTool {},
            });
        };
        if (!ChecksTraits<GeneralCheck>::visit_set_bits(failed_general_checks, v)) {
            return false;
        }
    }

    for (VirtualToolIndex tool : VirtualToolIndex::all()) {
        const auto v = [&](const CheckMetadata &meta) {
            return visitor(FailedCheck {
                .meta = &meta,
                .tool = tool,
            });
        };
        if (!ChecksTraits<VirtualToolCheck>::visit_set_bits(failed_virtual_tool_checks[tool], v)) {
            return false;
        }
    }
    for (GcodeToolIndex tool : GcodeToolIndex::all()) {
        const auto v = [&](const CheckMetadata &meta) {
            return visitor(FailedCheck {
                .meta = &meta,
                .tool = tool,
            });
        };
        if (!ChecksTraits<GCodeToolCheck>::visit_set_bits(failed_gcode_tool_checks[tool], v)) {
            return false;
        }
    }

    return true;
}

std::optional<CompatibilityReport::FailedCheck> CompatibilityReport::highest_severity_failed_check(std::optional<FailedCheck::Tool> check_filter) const {
    struct {
        std::optional<CompatibilityReport::FailedCheck> check;
        HWCheckSeverity severity = HWCheckSeverity::Ignore;
    } result;

    visit_failed_checks([&](const FailedCheck &check) {
        if (check_filter.has_value() && check.tool != check_filter) {
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

HWCheckSeverity CompatibilityReport::failure_severity() const {
    const auto check = highest_severity_failed_check();
    if (!check) {
        return HWCheckSeverity::Ignore;
    }

    return check->meta->evaluate_severity();
}

const GCodeInfo &CompatibilityReport::default_gcode_info() {
    return GCodeInfo::getInstance();
}

#if HAS_TOOL_MAPPING()
const ToolMapper &CompatibilityReport::default_tool_mapper() {
    return tool_mapper;
}
#endif

#if HAS_SPOOL_JOIN()
const SpoolJoin &CompatibilityReport::default_spool_join() {
    return spool_join;
}
#endif

} // namespace buddy::gcode_compatibility

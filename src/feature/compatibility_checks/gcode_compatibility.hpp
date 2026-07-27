/// @file
#pragma once

#include <optional>

#include <utils/enum_array.hpp>
#include <tool_index.hpp>
#include <inplace_function.hpp>
#include <string_view_utf8.hpp>
#include <marlin_server_types/general_response.hpp>

#include <option/has_gcode_compatibility.h>
#include <option/has_indx.h>
#include <option/has_mmu2.h>
#include <option/has_anfc.h>

#include "compatibility_checks_common.hpp"

#include <option/has_tool_mapping.h>
#if HAS_TOOL_MAPPING()
// Do not include the header, prevent dependency hell
class ToolMapper;
#endif

#include <option/has_spool_join.h>
#if HAS_SPOOL_JOIN()
// Do not include the header, prevent dependency hell
class SpoolJoin;
#endif

// Do not include the header, prevent dependency hell
class GCodeInfo;

namespace buddy::gcode_compatibility {

using namespace buddy::compatibility_checks;

enum class GeneralCheck : uint8_t {
    /// Fails if the gcode is not compatible at all, not even in compatibility mode
    /// Checked by M862.2 or M862.3 or printer_model (from comments)
    printer_model,

#if HAS_GCODE_COMPATIBILITY()
    /// Fails if any gcode compatibility mode needs to be applied
    gcode_compatibility_mode,
#endif

    /// I have no idea what this is for
    /// Checked by M862.5
    gcode_level,

    /// Fails if the gcode minimum FW version is higher than ours
    /// Checked by M862.4
    minimum_fw_version,

    /// Fails if the gcode was not sliced with input shaper support
    input_shaper,

#if HAS_INDX()
    /// Fails if the gcode was not sliced with INDX lock support
    indx_lock,
#endif

#if HAS_MMU2()
    /// Fails if the gcode requires MMU and we don't have it
    mmu,
#endif

    /// The GCode requests some features the printer doesn't have
    unsupported_features,

    /// Fails if gcode uses more tools than there are enabled tools
    not_enough_tools,

#if HAS_INDX()
    /// Fails if the nozzle cleaner is not calibrated
    nozzle_cleaner_not_calibrated,
#endif

    _cnt
};

enum class VirtualToolCheck : uint8_t {
    /// The mapped tool needs to be available (and of the right type)
    correct_tool,

    /// Whether the tool has the right nozzle diameter
    /// Checked by M862.1
    nozzle_diameter,

    /// Whether the nozzle is hardened, if the gcode requires it
    /// Checked by M862.1
    nozzle_hardened,

    /// Fails if the gcode is sliced for the HF nozzle and we don't have it
    /// Checked by M862.1
    nozzle_high_flow,

    /// Fails if the nozzle is high flow and the g-code is sliced for a non-HF one
    /// Checked only for the MMU prints.
    /// With MMU:
    /// - Slicing with a non-HF nozzle while HF nozzle is installed results in unsufficient purging.
    /// - Slicing for a HF nozzle without having it leads to extruder skipping.
    /// Checked by M862.1
    nozzle_not_high_flow,

    /// Fails if a filament is not loaded into the tool
    /// This is checked through the filament sensor
    filament_loaded,

    /// Filament type in gcodeinfo matches the filamenttype loaded to the tool
    filament_type,

    /// Fails if the G-code filament is incompatible with the installed hotend
    /// (e.g., PPS or PPA on standard hotend — HT hotend accepts all filament types)
    filament_hotend_compatible,

#if HAS_SPOOL_JOIN()
    /// Fails if the spool join is not possible
    can_spool_join,
#endif

#if HAS_INDX()
    /// Fails if the filament parameters are not calibrated (should happen during the load procedure)
    filament_calibrated,
#endif

    _cnt
};

enum class GCodeToolCheck : uint8_t {
    /// Fails if the gcode tool is not assigned to anything
    tool_assigned,

#if HAS_ANFC()
    /// Fails if there is not enough filament on the spools
    enough_filament,
#endif

    _cnt
};

struct CompatibilityReport : public CompatibilityReportBase<CompatibilityReport> {
    ChecksTraits<GeneralCheck>::Bitset failed_general_checks;
    StrongIndexArray<ChecksTraits<VirtualToolCheck>::Bitset, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static> failed_virtual_tool_checks;
    StrongIndexArray<ChecksTraits<GCodeToolCheck>::Bitset, GcodeToolIndex::count, GcodeToolIndex, GcodeToolIndex::to_raw_static> failed_gcode_tool_checks;

#if HAS_SPOOL_JOIN()
    /// True if the set up does a spool join
    bool does_spool_join : 1 = false;
#endif

    struct FailedCheck {
        using Tool = std::variant<VirtualToolIndex, GcodeToolIndex, NoTool>;

        const CheckMetadata *meta;
        Tool tool;
    };

    /// @returns false if the iteration should stop
    using FailedCheckVisitor = stdext::inplace_function<bool(const FailedCheck &)>;

    enum class AggregateTools {
        no,

        /// If selected, the visitor will aggregate all tools failed checks into one
        /// Failed checks will not repeat.
        /// FailedCheck will always have .tool = NoTool{}
        yes
    };

    /// @returns false if the iteration stopped by visitor returning false
    bool visit_failed_checks(const FailedCheckVisitor &visitor, AggregateTools aggregate_tools = AggregateTools::no) const;

    using CompatibilityReportBase::highest_severity_failed_check;

    auto highest_severity_failed_check(FailedCheck::Tool tool) const {
        return CompatibilityReportBase::highest_severity_failed_check_filtered([tool](const FailedCheck &check) { return check.tool == tool; });
    }

    bool failed(GeneralCheck check) const {
        return failed_general_checks.test(check);
    }

    bool failed(VirtualToolCheck check, VirtualToolIndex tool) const {
        return failed_virtual_tool_checks[tool].test(check);
    }
    bool failed(VirtualToolCheck check) const {
        return std::ranges::any_of(VirtualToolIndex::all(), [&](auto tool) { return failed(check, tool); });
    }

    bool failed(GCodeToolCheck check, GcodeToolIndex tool) const {
        return failed_gcode_tool_checks[tool].test(check);
    }
    bool failed(GCodeToolCheck check) const {
        return std::ranges::any_of(GcodeToolIndex::all(), [&](auto tool) { return failed(check, tool); });
    }

    static const GCodeInfo &default_gcode_info();
#if HAS_TOOL_MAPPING()
    static const ToolMapper &default_tool_mapper();
#endif
#if HAS_SPOOL_JOIN()
    static const SpoolJoin &default_spool_join();
#endif

    /// Checks compatibility of GCodeInfo against the current printer state.
    /// Stores the result in the CompatibilityReport itself.
    /// Does not report things that might be affected by toolmapping
    void generate_without_toolmapping(const GCodeInfo &gcode_info = default_gcode_info());

    struct ToolMappingArgs {
        const GCodeInfo &gcode_info = default_gcode_info();

#if HAS_TOOL_MAPPING()
        const ToolMapper &tool_mapper = default_tool_mapper();
#endif
#if HAS_SPOOL_JOIN()
        const SpoolJoin &spool_join = default_spool_join();
#endif
    };

    /// Checks compatibility of GCodeInfo against the current printer and tool mapping state.
    /// Stores the result in the CompatibilityReport itself.
    /// Only considers things that might be affected by toolmapping.
    void generate_toolmapping_only(const ToolMappingArgs &args);

    /// generate_without_toolmapping + generate_toolmapping_only
    void generate_full(const ToolMappingArgs &args);

    /// If there is a failed check with abort severity, shows that one.
    /// Otherwise shows a warning for each failed check with the warning severity.
    /// The user needs to confirm ignoring all of the warnings.
    /// Some warning ignores can change printer state (for example filament not loaded disables FS)
    /// @returns true if the user confirmed to skip all warnings
    /// !!! TO BE EXECUTED FROM THE GUI THREAD ONLY
    [[nodiscard]] bool gui_confirm_all_incompatibilities(Response abort_response = Response::Abort) const;

private:
    void generate_toolmapping_only_noclear(const ToolMappingArgs &args);
};

} // namespace buddy::gcode_compatibility

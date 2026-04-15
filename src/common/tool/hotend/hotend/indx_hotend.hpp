/// @file
#pragma once

#include "base_hotend.hpp"
#include <tool/tool/standard_fff_physical_tool.hpp>

/// Represents a hotend that is managed by a dwarf on an XL
class IndxHotend final : public BaseHotend {
    friend class PrusaToolChanger; // For access to stored_nozzle_target_temp_

public:
    /// !!! Careful, the config pointer is stored, so make sure the config is persistent!
    explicit IndxHotend(PhysicalToolIndex tool, const Config *config)
        : BaseHotend(tool, config) {}

    static StandardFFFPhysicalTool<IndxHotend> &indx_tool(PhysicalToolIndex tool);

public:
    void set_nozzle_target_temp(TargetTemperature set) override;

protected:
    virtual void manage() override;

    /// Sets the nozzle target temp without checking if the tool is picked.
    /// !!! Use only in toolchanger during pickup !!!
    void set_nozzle_target_temp_unchecked(TargetTemperature set);
    /// This is used for saving the target temp when the tool is parked and then restored on pickup.
    TargetTemperature stored_nozzle_target_temp_ = 0; // INDX_TODO: Add to power panic
};

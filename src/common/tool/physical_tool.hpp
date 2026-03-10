/// @file
#pragma once

#include "physical_tool_type.hpp"

#include <tool_index.hpp>

class Hotend;

struct FilamentTypeParameters;

/// Base class for all tools
/// !!! ALL PUBLIC FUNCTIONS MUST BE THREAD-SAFE UNLESS STATED OTHERWISE
class PhysicalTool {

public:
    /// @returns Relevant PhysicalTool
    /// There should be a 1:1 mapping
    /// Implemented differently for each printer in in tools_XX.cpp
    static PhysicalTool &for_index(PhysicalToolIndex tool);

    static PhysicalTool &for_index(std::variant<PhysicalToolIndex, NoTool> tool_index);

public:
    ToolType type() const {
        return type_;
    }

    /// !!! NOT THREAD SAFE. USE FROM MARLIN THREAD ONLY
    Hotend &hotend() {
        return hotend_ref_;
    }

    /// !!! NOT THREAD SAFE. USE FROM MARLIN THREAD ONLY
    const Hotend &hotend() const {
        return hotend_ref_;
    }

    virtual bool supports_filament(const FilamentTypeParameters &filament) const = 0;

protected:
    PhysicalTool(ToolType type, Hotend &hotend);

protected:
    const ToolType type_;

    Hotend &hotend_ref_;
};

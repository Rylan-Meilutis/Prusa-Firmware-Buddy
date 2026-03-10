/// @file
#include "standard_fff_physical_tool.hpp"

#include <tool/hotend/hotend.hpp>

StandardFFFPhysicalToolBase::StandardFFFPhysicalToolBase(Hotend &hotend)
    : PhysicalTool(ToolType::standard_fff, hotend) {}

bool StandardFFFPhysicalToolBase::supports_filament(const FilamentTypeParameters &filament) const {
    return hotend().supports_filament(filament);
}

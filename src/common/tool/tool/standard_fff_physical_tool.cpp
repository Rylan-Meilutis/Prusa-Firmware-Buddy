/// @file
#include "standard_fff_physical_tool.hpp"

#include <tool/hotend/hotend.hpp>
#include <feature/compatibility_checks/filament_compatibility.hpp>

#if BOARD_IS_MASTER_BOARD()
    #include <config_store/store_instance.hpp>
#endif

StandardFFFPhysicalToolBase::StandardFFFPhysicalToolBase(Hotend &hotend)
    : PhysicalTool(ToolType::standard_fff, hotend) {}

#if BOARD_IS_MASTER_BOARD()
void StandardFFFPhysicalToolBase::filament_compatibility_report(const FilamentTypeParameters &filament, FilamentCompatibilityReport &report) const {
    hotend().filament_compatibility_report(filament, report);
}
#endif

/// @file
#include "dummy_physical_tool.hpp"

DummyPhysicalTool::DummyPhysicalTool()
    : PhysicalTool(ToolType::none, hotend_) {
}

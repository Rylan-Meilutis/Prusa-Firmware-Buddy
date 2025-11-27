#include "tool_index.hpp"
#include <printers.h>
#include <module/prusa/toolchanger.h>
#include <option/has_tool_mapping.h>
#include <module/prusa/tool_mapper.hpp>

template <>
PhysicalToolIndex VirtualToolIndexExtension<VirtualToolIndex>::to_physical() const {
    const auto &self = static_cast<const VirtualToolIndex &>(*this);
    if constexpr (HAS_TOOLCHANGER()) {
        static_assert(!HAS_TOOLCHANGER() || PhysicalToolIndex::count == VirtualToolIndex::count);
        return PhysicalToolIndex::from_raw(self.to_raw());
    } else {
        static_assert(HAS_TOOLCHANGER() || PhysicalToolIndex::count == 1);
        return PhysicalToolIndex::from_raw(0);
    }
}

template <>
std::variant<PhysicalToolIndex, NoTool> PhysicalToolIndexExtension<PhysicalToolIndex>::from_raw_notool(uint8_t index) {
#if PRINTER_IS_PRUSA_XL()
    // count on XL is set to EXTRUDERS - 1 / HOTENDS - 1,
    // because of legacy definition of EXTRUDERS / HOTENDS to 6 instead of 5.
    // Values 0 to 4 are actual tools, 5 represents no tool (using MARLIN_NO_TOOL_PICKED).
    // If we encounter MARLIN_NO_TOOL_PICKED we need to return NoTool and check for it on call site.
    static_assert(PhysicalToolIndex::count == PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
    if (index == PrusaToolChanger::MARLIN_NO_TOOL_PICKED) {
        return NoTool {};
    }
#endif
    // Other non-tool values treat as invalid values -> raise bsod
    return PhysicalToolIndex::from_raw(index);
}

template <>
std::variant<VirtualToolIndex, NoTool> VirtualToolIndexExtension<VirtualToolIndex>::from_raw_notool(uint8_t index) {
#if HAS_TOOL_MAPPING()
    // There is 255 (NO_TOOL_MAPPED) used as no tool at some places.
    if (index == ToolMapper::NO_TOOL_MAPPED) {
        return NoTool {};
    }
#endif
    // Other non-tool values treat as invalid values -> raise bsod
    return VirtualToolIndex::from_raw(index);
}

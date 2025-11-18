#include "tool_index.hpp"

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

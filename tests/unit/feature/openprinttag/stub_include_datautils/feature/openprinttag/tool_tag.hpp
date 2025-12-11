#pragma once

#include <utility>

#include <feature/openprinttag/detail/defines.hpp>

namespace buddy::openprinttag {

struct ToolTag {};

struct ToolTagField {
    inline ToolTagField(const ToolTagField &) = default;

    template <CField Field>
    inline ToolTagField(Field field)
        : section(field_section(field))
        , field(std::to_underlying(field)) {
    }

    Section section;
    Field field;

    constexpr inline bool operator==(const ToolTagField &) const = default;
};

struct ToolTagFieldHash {
    using hash_type = std::hash<ToolTagField>;
    using is_transparent = void;

    std::size_t operator()(ToolTagField f) const {
        return std::hash<Section>()(f.section) ^ std::hash<Field>()(f.field);
    }
};

} // namespace buddy::openprinttag

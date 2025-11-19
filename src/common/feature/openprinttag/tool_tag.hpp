/// @file
#pragma once

#include <cstdint>
#include <optional>

#include <tool_index.hpp>

#include "defines.hpp"

namespace buddy::openprinttag {

struct ToolTagField;

/// Class representing a specific tag associated with a specific tool
/// The value gets "invalidated" if a tag is removed from the tool
class ToolTag {

public:
    using AssignSeq = uint16_t;

public:
    /// @returns tag assigned to the specified tool, if there is any
    static std::optional<ToolTag> for_tool(VirtualToolIndex tool);

public:
    /// @returns a struct representing a specific field on the tag
    template <typename F>
    inline ToolTagField field(F field) const;

private:
    VirtualToolIndex tool_;

    /// Tag assignment ID for the specified tool.
    /// This value changes every time the tag for the specified tool is changed
    /// This ensures that when we issue multiple read requests, they are all read from the same NFC tag, or they fail.
    AssignSeq assign_seq_;
};

struct ToolTagField {
    ToolTag tag;
    Section section;
    Field field;
};

template <typename F>
inline ToolTagField ToolTag::field(F field) const {
    return ToolTagField {
        .tag = *this,
        .section = ::openprinttag::field_section(field),
        .field = static_cast<Field>(field),
    };
}

}; // namespace buddy::openprinttag

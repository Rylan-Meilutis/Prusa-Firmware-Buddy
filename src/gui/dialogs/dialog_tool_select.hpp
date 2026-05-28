/// @file
#pragma once

#include <option/has_toolchanger.h>
static_assert(HAS_TOOLCHANGER());

#include <tool_index.hpp>
#include <window_menu_virtual.hpp>

struct SelectToolDialogArgs {
    /// Whether to show the return button or not
    bool allow_return : 1;

    using ToolFilter = stdext::inplace_function<bool(VirtualToolIndex)>;

    static constexpr auto default_tool_filter = [](VirtualToolIndex) { return true; };

    /// Additional filter that can disable more tools by returning false
    ToolFilter tool_filter = default_tool_filter;

    /// Number of items in the custom section that is above the individual tool items
    int prefix_section_size = 0;

    /// Constructor for the prefix section.
    /// @param index is the item index within the section
    stdext::inplace_function<void(WindowMenuVirtual::ItemVariant &item, int index)> prefix_section_ctor {};
};

/// Shows a dialog that prompts the user to select a tool
/// @returns the selected tool or std::nullopt if the user did the return
std::optional<VirtualToolIndex> select_tool_dialog(const SelectToolDialogArgs &args);

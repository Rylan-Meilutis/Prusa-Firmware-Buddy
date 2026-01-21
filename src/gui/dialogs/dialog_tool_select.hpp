/// @file
#pragma once

#include <option/has_toolchanger.h>
static_assert(HAS_TOOLCHANGER());

#include <tool_index.hpp>

struct SelectToolDialogArgs {
    /// Whether to show the return button or not
    bool allow_return : 1;

    using ToolFilter = stdext::inplace_function<bool(VirtualToolIndex)>;

    static constexpr auto default_tool_filter = [](VirtualToolIndex) { return true; };

    /// Additional filter that can disable more tools by returning false
    ToolFilter tool_filter = default_tool_filter;
};

/// Shows a dialog that prompts the user to select a tool
/// @returns the selected tool or std::nullopt if the user did the return
std::optional<VirtualToolIndex> select_tool_dialog(const SelectToolDialogArgs &args);

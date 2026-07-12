#include "screen_filaments_loaded.hpp"

#include <filament_gui.hpp>
#include <utils/string_builder.hpp>
#include <print_utils.hpp>
#include <ScreenHandler.hpp>

MI_LOADED_FILAMENT::MI_LOADED_FILAMENT(DisplayFormat display_format, uint8_t tool)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
    , display_format_(display_format)
    , tool_(tool) //
{
    should_open_submenu_ = (display_format == DisplayFormat::auto_submenu) && (get_num_of_enabled_tools() > 1);

    if (should_open_submenu_) {
        SetLabel(_("Loaded filaments"));

    } else {
        filament_type_ = config_store().get_filament_type(tool);

        StringBuilder sb(label_buffer_);
        if (display_format == DisplayFormat::auto_submenu) {
#if HAS_MINI_DISPLAY()
            // Longer text doesn't fit well on the mini display
            sb.append_string_view(_("Loaded"));
#else
            sb.append_string_view(_("Loaded filament"));
#endif
        } else {
            sb.append_string_view(_("Filament"));
            sb.append_printf(" %d", tool + 1);
        }

        sb.append_string(": ");
        filament_type_.build_name_with_info(sb);

        SetLabel(string_view_utf8::MakeRAM(label_buffer_.data()));
        set_is_hidden(!is_tool_enabled(tool));
    }
}

void MI_LOADED_FILAMENT::click(IWindowMenu &) {
    if (should_open_submenu_) {
        Screens::Access()->Open<ScreenLoadedFilaments>();
    } else {
        Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenAssignLoadedFilament>(tool_));
    }
}

ScreenLoadedFilaments::ScreenLoadedFilaments()
    : ScreenMenu(_("LOADED FILAMENTS")) {}

using namespace screen_loaded_filament_assignment;

MI_ASSIGN_LOADED_FILAMENT::MI_ASSIGN_LOADED_FILAMENT(uint8_t tool, FilamentType filament_type)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
    , tool_(tool)
    , filament_type_(filament_type)
    , filament_name_(filament_type.parameters().name) {
    if (filament_type_ == FilamentType::none) {
        SetLabel(_("None"));
    } else {
        FilamentTypeGUI::setup_menu_item(filament_type_, filament_name_, *this);
    }
}

void MI_ASSIGN_LOADED_FILAMENT::click(IWindowMenu &) {
    config_store().set_filament_type(tool_, filament_type_);
    Screens::Access()->Close();
}

WindowMenuAssignLoadedFilament::WindowMenuAssignLoadedFilament(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes)
    , tool_(0) {
}

void WindowMenuAssignLoadedFilament::set_tool(uint8_t tool) {
    tool_ = tool;
    generate_filament_list(filament_list_, management_generate_filament_list_config);
    setup_items();
}

int WindowMenuAssignLoadedFilament::item_count() const {
    return 2 + filament_list_.size();
}

void WindowMenuAssignLoadedFilament::setup_item(ItemVariant &variant, int index) {
    if (index == 0) {
        variant.emplace<MI_RETURN>();
    } else if (index == 1) {
        variant.emplace<MI_ASSIGN_LOADED_FILAMENT>(tool_, FilamentType::none);
    } else {
        variant.emplace<MI_ASSIGN_LOADED_FILAMENT>(tool_, filament_list_[index - 2]);
    }
}

ScreenAssignLoadedFilament::ScreenAssignLoadedFilament(uint8_t tool)
    : ScreenMenuBase(nullptr, _("ASSIGN FILAMENT"), EFooter::Off) {
    menu.menu.set_tool(tool);
}

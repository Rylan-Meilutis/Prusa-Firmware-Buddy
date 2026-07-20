#include "screen_filaments_loaded.hpp"

#include <filament_gui.hpp>
#include <utils/string_builder.hpp>
#include <print_utils.hpp>
#include <ScreenHandler.hpp>
#include <dialog_text_input.hpp>

namespace {
struct PendingLoadedFilament {
    uint8_t tool = 0;
    FilamentType material = FilamentType::none;
    std::optional<Color> color;
} pending;

void begin_edit(const uint8_t tool) {
    pending = { tool, config_store().get_filament_type(tool), filament_color::loaded(tool) };
}
}

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
        sb.append_string(filament_type_.parameters().name.data());
        if (const auto color = filament_color::loaded(tool)) {
            const auto color_profile = filament_color::profile_for(*color);
            sb.append_string(" / ");
            sb.append_string(color_profile.name.data());
        }

        SetLabel(string_view_utf8::MakeRAM(label_buffer_.data()));
        set_is_hidden(!is_tool_enabled(tool));
    }
}

void MI_LOADED_FILAMENT::click(IWindowMenu &) {
    if (should_open_submenu_) {
        Screens::Access()->Open<ScreenLoadedFilaments>();
    } else {
        begin_edit(tool_);
        Screens::Access()->Open<screen_loaded_color_assignment::ScreenEditLoadedFilament>();
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
    pending.material = filament_type_;
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

using namespace screen_loaded_color_assignment;

MI_ASSIGN_LOADED_COLOR::MI_ASSIGN_LOADED_COLOR(uint8_t tool, std::optional<Color> color, std::string_view name)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no), tool_(tool), color_(color) {
#if HAS_MINI_DISPLAY()
    SetLabel(color ? string_view_utf8::MakeCPUFLASH(name.data()) : string_view_utf8::MakeCPUFLASH("None"));
#else
    if (color) snprintf(label_.data(), label_.size(), "%.*s  #%06lx", static_cast<int>(name.size()), name.data(), static_cast<unsigned long>(color->raw & 0xffffff));
    else snprintf(label_.data(), label_.size(), "%s", "None");
    SetLabel(string_view_utf8::MakeRAM(label_.data()));
#endif
}

void MI_ASSIGN_LOADED_COLOR::click(IWindowMenu &) {
    pending.color = color_;
    Screens::Access()->Close();
}

WindowMenuAssignLoadedColor::WindowMenuAssignLoadedColor(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes), tool_(0) {}

void WindowMenuAssignLoadedColor::set_tool(uint8_t tool) {
    tool_ = tool;
    custom_count_ = 0;
#if !HAS_MINI_DISPLAY()
    for (size_t i = 0; i < filament_color::custom_slot_count; ++i) custom_count_ += filament_color::custom(i).has_value();
#endif
    setup_items();
}

int WindowMenuAssignLoadedColor::item_count() const {
    return 2 + filament_color::palette().size() + custom_count_;
}

void WindowMenuAssignLoadedColor::setup_item(ItemVariant &variant, int index) {
    if (index == 0) { variant.emplace<MI_RETURN>(); return; }
    if (index == 1) { variant.emplace<MI_ASSIGN_LOADED_COLOR>(tool_, std::nullopt, std::string_view("None")); return; }
    size_t requested = static_cast<size_t>(index - 2);
    if (requested < filament_color::palette().size()) {
        const auto &p = filament_color::palette()[requested];
        variant.emplace<MI_ASSIGN_LOADED_COLOR>(tool_, p.color, p.name_view());
        return;
    }
#if !HAS_MINI_DISPLAY()
    requested -= filament_color::palette().size();
    for (size_t slot = 0; slot < filament_color::custom_slot_count; ++slot) {
        if (const auto p = filament_color::custom(slot); p && requested-- == 0) {
            variant.emplace<MI_ASSIGN_LOADED_COLOR>(tool_, p->color, p->name_view());
            return;
        }
    }
#endif
}

ScreenAssignLoadedColor::ScreenAssignLoadedColor(uint8_t tool)
    : ScreenMenuBase(nullptr, _("ASSIGN COLOR"), EFooter::Off) {
    menu.menu.set_tool(tool);
}

MI_EDIT_LOADED_MATERIAL::MI_EDIT_LOADED_MATERIAL()
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
    refresh();
}

void MI_EDIT_LOADED_MATERIAL::refresh() {
    snprintf(label_.data(), label_.size(), "Material: %s", pending.material.parameters().name.data());
    SetLabel(string_view_utf8::MakeRAM(label_.data()));
}

void MI_EDIT_LOADED_MATERIAL::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenAssignLoadedFilament>(pending.tool));
}

MI_EDIT_LOADED_COLOR::MI_EDIT_LOADED_COLOR()
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
    refresh();
}

void MI_EDIT_LOADED_COLOR::refresh() {
    const auto profile = pending.color ? filament_color::profile_for(*pending.color) : filament_color::Profile {};
    const auto name = pending.color ? profile.name_view() : std::string_view("None");
    snprintf(label_.data(), label_.size(), "Color: %.*s", static_cast<int>(name.size()), name.data());
    SetLabel(string_view_utf8::MakeRAM(label_.data()));
}

void MI_EDIT_LOADED_COLOR::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenAssignLoadedColor>(pending.tool));
}

MI_SAVE_LOADED_FILAMENT::MI_SAVE_LOADED_FILAMENT()
    : IWindowMenuItem(_("Save"), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_SAVE_LOADED_FILAMENT::click(IWindowMenu &) {
    config_store().set_filament_type(pending.tool, pending.material);
    filament_color::set_loaded(pending.tool, pending.color);
    Screens::Access()->Close();
}

ScreenEditLoadedFilament::ScreenEditLoadedFilament()
    : ScreenEditLoadedFilament_(_("LOADED FILAMENT")) {}

void ScreenEditLoadedFilament::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    Item<MI_EDIT_LOADED_MATERIAL>().refresh();
    Item<MI_EDIT_LOADED_COLOR>().refresh();
    ScreenEditLoadedFilament_::windowEvent(sender, event, param);
}

MI_ADD_CUSTOM_COLOR::MI_ADD_CUSTOM_COLOR()
    : IWindowMenuItem(_("Add New Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_ADD_CUSTOM_COLOR::click(IWindowMenu &) {
    size_t slot = 0;
    while (slot < filament_color::custom_slot_count && filament_color::custom(slot)) ++slot;
    if (slot == filament_color::custom_slot_count) {
        MsgBoxWarning(string_view_utf8::MakeCPUFLASH("All custom color slots are in use."), Responses_Ok);
        return;
    }
    std::array<char, filament_color::name_capacity> name {};
    std::array<char, 8> hex { '#', '0', '0', '0', '0', '0', '0', '\0' };
    if (!DialogTextInput::exec(_("Color name"), name) || !DialogTextInput::exec(_("Color picker #RRGGBB"), hex, false, true)) return;
    const auto color = Color::from_string(hex.data());
    if (!color || !filament_color::set_custom(slot, name.data(), *color)) {
        MsgBoxWarning(string_view_utf8::MakeCPUFLASH("Enter a name and valid #RRGGBB color."), Responses_Ok);
        return;
    }
    Screens::Access()->Close();
}

MI_SAVED_CUSTOM_COLOR::MI_SAVED_CUSTOM_COLOR(const size_t slot)
    : IWindowMenuItem({}, nullptr, is_enabled_t::no, is_hidden_t::no) {
    if (const auto profile = filament_color::custom(slot)) {
        snprintf(label_.data(), label_.size(), "%.*s  #%06lx", static_cast<int>(profile->name_view().size()), profile->name.data(), static_cast<unsigned long>(profile->color.raw & 0xffffff));
        SetLabel(string_view_utf8::MakeRAM(label_.data()));
    }
}

WindowMenuCustomFilamentColors::WindowMenuCustomFilamentColors(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {
    for (size_t slot = 0; slot < filament_color::custom_slot_count; ++slot) saved_count_ += filament_color::custom(slot).has_value();
    setup_items();
}

int WindowMenuCustomFilamentColors::item_count() const { return 2 + saved_count_; }

void WindowMenuCustomFilamentColors::setup_item(ItemVariant &variant, int index) {
    if (index == 0) { variant.emplace<MI_RETURN>(); return; }
    if (index == 1) { variant.emplace<MI_ADD_CUSTOM_COLOR>(); return; }
    size_t requested = index - 2;
    for (size_t slot = 0; slot < filament_color::custom_slot_count; ++slot) {
        if (filament_color::custom(slot) && requested-- == 0) {
            variant.emplace<MI_SAVED_CUSTOM_COLOR>(slot);
            return;
        }
    }
}

ScreenCustomFilamentColors::ScreenCustomFilamentColors()
    : ScreenMenuBase(nullptr, _("CUSTOM FILAMENT COLORS"), EFooter::Off) {}

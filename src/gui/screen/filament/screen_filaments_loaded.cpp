#include "screen_filaments_loaded.hpp"

#include <filament_gui.hpp>
#include <utils/string_builder.hpp>
#include <print_utils.hpp>
#include <ScreenHandler.hpp>
#include <img_resources.hpp>
#include <dialog_text_input.hpp>
#include <filament_color_gui.hpp>

#include <option/has_anfc.h>
#if HAS_ANFC()
    #include <feature/openprinttag/tool_tag.hpp>
#endif

namespace {
#if !HAS_MINI_DISPLAY()
struct PendingLoadedFilament {
    VirtualToolIndex tool = VirtualToolIndex::from_raw(0);
    FilamentType material = FilamentType::none;
    std::optional<Color> color;
} pending;

void begin_edit(const VirtualToolIndex tool) {
    pending = { tool, config_store().get_filament_type(tool), filament_color::loaded(tool.to_raw()) };
}
#endif
}

MI_LOADED_FILAMENT::MI_LOADED_FILAMENT(DisplayFormat display_format, uint8_t tool)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
    , tool_(VirtualToolIndex::from_raw(tool))
    , display_format_(display_format) {

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
            sb.append_printf(" %d", tool_.display_index());
        }

        sb.append_string(": ");
        sb.append_string(filament_type_.parameters().name.data());
        color_ = filament_color::loaded(tool);
        if (color_) {
            const auto color_profile = filament_color::profile_for(*color_);
            sb.append_string(" / ");
            sb.append_string(color_profile.name.data());
            extension_width = filament_color_gui::swatch_and_arrow_extension_width;
        }

        SetLabel(string_view_utf8::MakeRAM(label_buffer_.data()));
        set_is_hidden(!tool_.is_enabled());
    }
}

void MI_LOADED_FILAMENT::printExtension(Rect16 extension_rect, Color color_text, Color color_back, ropfn raster_op) const {
    if (!color_) {
        IWindowMenuItem::printExtension(extension_rect, color_text, color_back, raster_op);
        return;
    }

    Rect16 swatch_rect = extension_rect;
    swatch_rect -= Rect16::Width_t { expand_icon_width };
    filament_color_gui::draw_swatch(swatch_rect, *color_, color_back);

    extension_rect += Rect16::Left_t { static_cast<int16_t>(extension_rect.Width() - expand_icon_width) };
    extension_rect = Rect16::Width_t { expand_icon_width };
    IWindowMenuItem::printExtension(extension_rect, color_text, color_back, raster_op);
}

void MI_LOADED_FILAMENT::click(IWindowMenu &) {
    if (should_open_submenu_) {
        Screens::Access()->Open<ScreenLoadedFilaments>();
    } else {
#if HAS_MINI_DISPLAY()
        Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenAssignLoadedFilament>(tool_.to_raw()));
#else
        begin_edit(tool_);
        Screens::Access()->Open<screen_loaded_color_assignment::ScreenEditLoadedFilament>();
#endif
    }
}

void MI_LOADED_FILAMENT::Loop() {
#if HAS_ANFC()
    if (!should_open_submenu_) {
        using buddy::openprinttag::ToolTag;
        const auto ephemeral_tag = ToolTag::for_tool_ephemeral(tool_);
        const auto assigned_tag = ToolTag::for_tool_assigned(tool_);

        if (filament_type_ != FilamentType::none && ephemeral_tag != assigned_tag) {
            // Assigned OpenPrintTag is different to what is currently present
            SetIconId(&img::openprinttag_orange_16x16);

        } else if (ephemeral_tag.has_value()) {
            // Tool has a tag assigned and it is present
            SetIconId(&img::openprinttag_white_16x16);

        } else {
            // No tag present && no tag assigned
            SetIconId(nullptr);
        }
    }
#endif
}

ScreenLoadedFilaments::ScreenLoadedFilaments()
    : ScreenMenu(_("LOADED FILAMENTS")) {}

using namespace screen_loaded_filament_assignment;

MI_ASSIGN_LOADED_FILAMENT::MI_ASSIGN_LOADED_FILAMENT(VirtualToolIndex tool, FilamentType filament_type)
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
#if HAS_MINI_DISPLAY()
    config_store().set_filament_type(tool_, filament_type_);
#else
    pending.material = filament_type_;
#endif
    Screens::Access()->Close();
}

WindowMenuAssignLoadedFilament::WindowMenuAssignLoadedFilament(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes)
    , tool_(VirtualToolIndex::from_raw(0)) {
}

void WindowMenuAssignLoadedFilament::set_tool(VirtualToolIndex tool) {
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
    menu.menu.set_tool(VirtualToolIndex::from_raw(tool));
}

#if !HAS_MINI_DISPLAY()
using namespace screen_loaded_color_assignment;

MI_ASSIGN_LOADED_COLOR::MI_ASSIGN_LOADED_COLOR(VirtualToolIndex tool, std::optional<Color> color, std::string_view name)
    : IWindowMenuItem({}, color ? filament_color_gui::swatch_extension_width : Rect16::Width_t { 0 }, nullptr, is_enabled_t::yes, is_hidden_t::no), tool_(tool), color_(color) {
#if HAS_MINI_DISPLAY()
    SetLabel(color ? string_view_utf8::MakeCPUFLASH(name.data()) : string_view_utf8::MakeCPUFLASH("None"));
#else
    if (color) snprintf(label_.data(), label_.size(), "%.*s", static_cast<int>(name.size()), name.data());
    else snprintf(label_.data(), label_.size(), "%s", "None");
    SetLabel(string_view_utf8::MakeRAM(label_.data()));
#endif
}

void MI_ASSIGN_LOADED_COLOR::printExtension(Rect16 extension_rect, Color, Color color_back, ropfn) const {
    if (color_) filament_color_gui::draw_swatch(extension_rect, *color_, color_back);
}

void MI_ASSIGN_LOADED_COLOR::click(IWindowMenu &) {
    pending.color = color_;
    Screens::Access()->Close();
}

WindowMenuAssignLoadedColor::WindowMenuAssignLoadedColor(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes), tool_(VirtualToolIndex::from_raw(0)) {}

void WindowMenuAssignLoadedColor::set_tool(VirtualToolIndex tool) {
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
    menu.menu.set_tool(VirtualToolIndex::from_raw(tool));
}

MI_EDIT_LOADED_MATERIAL::MI_EDIT_LOADED_MATERIAL()
    : IWindowMenuItem(_("Material"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_EDIT_LOADED_MATERIAL::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenAssignLoadedFilament>(pending.tool.to_raw()));
}

MI_EDIT_LOADED_COLOR::MI_EDIT_LOADED_COLOR()
    : IWindowMenuItem(_("Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_EDIT_LOADED_COLOR::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenAssignLoadedColor>(pending.tool.to_raw()));
}

MI_SAVE_LOADED_FILAMENT::MI_SAVE_LOADED_FILAMENT()
    : IWindowMenuItem(_("Save"), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_SAVE_LOADED_FILAMENT::click(IWindowMenu &) {
    config_store().set_filament_type(pending.tool, pending.material);
    filament_color::set_loaded(pending.tool.to_raw(), pending.color);
    Screens::Access()->Close();
}

ScreenEditLoadedFilament::ScreenEditLoadedFilament()
    : ScreenEditLoadedFilament_(_("LOADED FILAMENT")) {}

MI_ADD_CUSTOM_COLOR::MI_ADD_CUSTOM_COLOR()
    : IWindowMenuItem(_("Add New Color"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_ADD_CUSTOM_COLOR::click(IWindowMenu &menu) {
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
    static_cast<WindowMenuCustomFilamentColors &>(menu).reload_colors();
}

MI_SAVED_CUSTOM_COLOR::MI_SAVED_CUSTOM_COLOR(const size_t slot)
    : IWindowMenuItem({}, filament_color_gui::swatch_extension_width, nullptr, is_enabled_t::no, is_hidden_t::no) {
    if (const auto profile = filament_color::custom(slot)) {
        color_ = profile->color;
        snprintf(label_.data(), label_.size(), "%.*s", static_cast<int>(profile->name_view().size()), profile->name.data());
        SetLabel(string_view_utf8::MakeRAM(label_.data()));
    }
}

void MI_SAVED_CUSTOM_COLOR::printExtension(Rect16 extension_rect, Color, Color color_back, ropfn) const {
    if (color_) filament_color_gui::draw_swatch(extension_rect, *color_, color_back);
}

WindowMenuCustomFilamentColors::WindowMenuCustomFilamentColors(window_t *parent, Rect16 rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::yes) {
    reload_colors();
}

void WindowMenuCustomFilamentColors::reload_colors() {
    saved_count_ = 0;
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
#endif

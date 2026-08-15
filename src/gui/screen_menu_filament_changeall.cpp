
#include "screen_menu_filament_changeall.hpp"

#include <algorithm_extensions.hpp>

#include <ScreenHandler.hpp>
#include <img_resources.hpp>
#include <marlin_client.hpp>
#include <option/has_mmu2.h>
#include <config_store/store_instance.hpp>
#include <utils/string_builder.hpp>
#include <algorithm_extensions.hpp>
#include <filament_list.hpp>
#include <option/has_toolchanger.h>
#include <filament_color.hpp>
#include <filament_manufacturer.hpp>
#include <window_menu_virtual.hpp>
#include <filament_color_gui.hpp>
#include <dialog_text_input.hpp>

using namespace multi_filament_change;

namespace {

class MI_PRELOAD_MANUFACTURER final : public IWindowMenuItem {
public:
    MI_PRELOAD_MANUFACTURER(MI_ActionSelect *owner, std::optional<uint8_t> id, std::string_view name)
        : IWindowMenuItem(string_view_utf8::MakeRAM(name.data()))
        , owner_(owner)
        , id_(id) {}

protected:
    void click(IWindowMenu &) override {
        owner_->set_selected_manufacturer(id_);
        Screens::Access()->Close();
        Screens::Access()->Close();
    }

private:
    MI_ActionSelect *owner_;
    std::optional<uint8_t> id_;
};

class MI_PRELOAD_NEW_MANUFACTURER final : public IWindowMenuItem {
public:
    explicit MI_PRELOAD_NEW_MANUFACTURER(MI_ActionSelect *owner)
        : IWindowMenuItem(_("Add Manufacturer"), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
        , owner_(owner) {}

protected:
    void click(IWindowMenu &) override {
        size_t slot = 0;
        while (slot < filament_manufacturer::custom_slot_count && filament_manufacturer::custom(slot)) {
            ++slot;
        }
        if (slot == filament_manufacturer::custom_slot_count) {
            MsgBoxWarning(_("All manufacturer slots are in use."), Responses_Ok);
            return;
        }
        std::array<char, filament_manufacturer::name_capacity> name {};
        if (!DialogTextInput::exec(_("Manufacturer"), name)
            || !filament_manufacturer::set_custom(slot, name.data())) {
            MsgBoxWarning(_("Enter a unique manufacturer name."), Responses_Ok);
            return;
        }
        const auto created = filament_manufacturer::custom(slot);
        owner_->set_selected_manufacturer(created ? std::optional<uint8_t> { created->id } : std::nullopt);
        Screens::Access()->Close();
        Screens::Access()->Close();
    }

private:
    MI_ActionSelect *owner_;
};

class WindowMenuPreloadManufacturer final : public WindowMenuVirtual {
public:
    WindowMenuPreloadManufacturer(window_t *parent, Rect16 rect)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::no) {}
    void set_owner(MI_ActionSelect *owner) {
        owner_ = owner;
        custom_count_ = 0;
        for (size_t i = 0; i < filament_manufacturer::custom_slot_count; ++i) {
            custom_count_ += filament_manufacturer::custom(i).has_value();
        }
        setup_items();
    }
    int item_count() const override { return 2 + filament_manufacturer::preset_count + custom_count_; }

protected:
    void setup_item(ItemVariant &variant, int index) override {
        if (index == 0) {
            variant.emplace<MI_PRELOAD_MANUFACTURER>(owner_, std::nullopt, std::string_view("None"));
            return;
        }
        if (index == item_count() - 1) {
            variant.emplace<MI_PRELOAD_NEW_MANUFACTURER>(owner_);
            return;
        }
        size_t requested = static_cast<size_t>(index - 1);
        if (requested < filament_manufacturer::preset_count) {
            variant.emplace<MI_PRELOAD_MANUFACTURER>(owner_, static_cast<uint8_t>(requested + 1), filament_manufacturer::preset(requested));
            return;
        }
        requested -= filament_manufacturer::preset_count;
        for (size_t slot = 0; slot < filament_manufacturer::custom_slot_count; ++slot) {
            if (const auto profile = filament_manufacturer::custom(slot); profile && requested-- == 0) {
                variant.emplace<MI_PRELOAD_MANUFACTURER>(owner_, profile->id, profile->name_view());
                return;
            }
        }
    }

private:
    MI_ActionSelect *owner_ = nullptr;
    size_t custom_count_ = 0;
};

class ScreenPreloadManufacturer final : public ScreenMenuBase<WindowMenuPreloadManufacturer> {
public:
    explicit ScreenPreloadManufacturer(MI_ActionSelect *owner)
        : ScreenMenuBase(nullptr, _("SELECT MANUFACTURER"), EFooter::On) { menu.menu.set_owner(owner); }
};

class MI_PRELOAD_COLOR final : public IWindowMenuItem {
public:
    MI_PRELOAD_COLOR(MI_ActionSelect *owner, std::optional<Color> color, std::string_view name)
        : IWindowMenuItem(string_view_utf8::MakeRAM(name.data()), color ? filament_color_gui::swatch_extension_width : Rect16::Width_t { 0 })
        , owner_(owner)
        , color_(color) {}

protected:
    void printExtension(Rect16 extension_rect, Color, Color color_back, ropfn) const override {
        if (color_) {
            filament_color_gui::draw_swatch(extension_rect, *color_, color_back);
        }
    }

    void click(IWindowMenu &) override {
        owner_->set_selected_color(color_);
        Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenPreloadManufacturer>(owner_));
    }

private:
    MI_ActionSelect *owner_;
    std::optional<Color> color_;
};

class WindowMenuPreloadColor final : public WindowMenuVirtual {
public:
    WindowMenuPreloadColor(window_t *parent, Rect16 rect)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::no) {}

    void set_owner(MI_ActionSelect *owner) {
        owner_ = owner;
        custom_count_ = 0;
        for (size_t i = 0; i < filament_color::custom_slot_count; ++i) {
            custom_count_ += filament_color::custom(i).has_value();
        }
        setup_items();
    }

    int item_count() const override { return 1 + filament_color::palette().size() + custom_count_; }

protected:
    void setup_item(ItemVariant &variant, int index) override {
        if (index == 0) {
            variant.emplace<MI_PRELOAD_COLOR>(owner_, std::nullopt, std::string_view("None"));
            return;
        }
        size_t requested = static_cast<size_t>(index - 1);
        if (requested < filament_color::palette().size()) {
            const auto &profile = filament_color::palette()[requested];
            variant.emplace<MI_PRELOAD_COLOR>(owner_, profile.color, profile.name_view());
            return;
        }
        requested -= filament_color::palette().size();
        for (size_t slot = 0; slot < filament_color::custom_slot_count; ++slot) {
            if (const auto profile = filament_color::custom(slot); profile && requested-- == 0) {
                variant.emplace<MI_PRELOAD_COLOR>(owner_, profile->color, profile->name_view());
                return;
            }
        }
    }

private:
    MI_ActionSelect *owner_ = nullptr;
    size_t custom_count_ = 0;
};

class ScreenPreloadColor final : public ScreenMenuBase<WindowMenuPreloadColor> {
public:
    explicit ScreenPreloadColor(MI_ActionSelect *owner)
        : ScreenMenuBase(nullptr, _("SELECT COLOR"), EFooter::On) {
        menu.menu.set_owner(owner);
    }
};

} // namespace

MI_ActionSelect::MI_ActionSelect(uint8_t tool_ix)
    : MenuItemSelectMenu({})
    , tool_filter_ { VirtualToolIndex::from_raw(tool_ix) } {
    const auto tool = VirtualToolIndex::from_raw(tool_ix);
    has_filament_loaded = (config_store().get_filament_type(tool) != FilamentType::none);
    set_is_hidden(!tool.is_enabled());
    SetLabel(tool.display_name(label_params));
}

MI_ActionSelect::MI_ActionSelect(SetAllToMode)
    : MenuItemSelectMenu(_("Set All To"))
    , tool_filter_ { AllTools {} } {
    set_behavior(Behavior::select_only);

    // Necessary to generate filament list
    set_config({});
}

void MI_ActionSelect::set_config(const ConfigItem &set, CompactOptional<Color, COLOR_NONE> set_color) {
    // By using enforce_first_item, we make sure the target filament is in the list (it might be hidden otherwise) and that it's on the first place (which is a welcome bonus)
    generate_filament_list(filament_list, {
                                              .enforce_first_item = set.new_filament,
                                              .compatible_with_tool = stdext::to_variant(tool_filter_),
                                          });
    index_mapping.set_section_size<Action::change>(filament_list.size());

    color = set_color;
    manufacturer = set.manufacturer;
    set_current_item([&] -> size_t {
        switch (set.action) {
        case Action::keep:
            return index_mapping.to_index<Action::keep>();

        case Action::unload:
            return index_mapping.to_index<Action::unload>();

        case Action::change:
            return index_mapping.to_index<Action::change>(stdext::index_of(filament_list, set.new_filament));
        }

        std::abort();
    }());
}

ConfigItem MI_ActionSelect::config(int item_index) const {
    const auto mapping = index_mapping.from_index(item_index);
    return ConfigItem {
        .action = mapping.item,
        .new_filament = (mapping.item == Action::change) ? filament_list[mapping.pos_in_section] : FilamentType::none,
        .manufacturer = manufacturer,
    };
}

int MI_ActionSelect::item_count() const {
    return index_mapping.total_item_count();
}

string_view_utf8 MI_ActionSelect::build_item_text(int index, MenuItemSelectMenu::ItemTextParams &params) const {
    const auto mapping = index_mapping.from_index(index);
    switch (mapping.item) {

    case Action::keep:
        return _("Don't change");

    case Action::unload:
        return _("Unload");

    case Action::change: {
        const auto fmt = has_filament_loaded ? N_("Change to %s") : N_("Load %s");
        return _(fmt).formatted(params, filament_list[mapping.pos_in_section].parameters().name.data());
    }
    }

    bsod_unreachable();
}

bool MI_ActionSelect::on_item_selected(const OnItemSelectedArgs &args) {
    if (std::holds_alternative<AllTools>(tool_filter_)) {
        auto &menu = static_cast<MenuMultiFilamentChange &>(args.menu);
        auto new_config = menu.configuration();
        const auto new_config_item = this->config(args.new_index);

        for (auto tool : VirtualToolIndex::all().skip_all_disabled()) {
            auto &config_item = new_config[tool];

            // Keep the color
            const auto orig_color = new_config.colors[tool];
            const auto orig_manufacturer = config_item.manufacturer;
            config_item = new_config_item;
            new_config.colors[tool] = orig_color;
            config_item.manufacturer = orig_manufacturer;
        }

        menu.set_configuration(new_config);

    } else {
        const auto selected = config(args.new_index);
        if (selected.action == Action::change) {
            // Commit the material row before opening another screen. The menu
            // selector normally commits only after this callback returns, but
            // opening the color screen changes the active screen immediately.
            set_current_item(args.new_index);
            Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenPreloadColor>(this));
            return false;
        } else if (selected.action == Action::unload) {
            color = std::nullopt;
            manufacturer = 0;
        }
    }

    return true;
}

MI_ApplyChanges::MI_ApplyChanges()
    : IWindowMenuItem(_("Carry Out the Changes"), &img::arrow_right_10x16, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ApplyChanges::click(IWindowMenu &menu) {
    menu.WindowEvent(&menu, GUI_event_t::CHILD_CLICK, nullptr);
}

MenuMultiFilamentChange::MenuMultiFilamentChange(window_t *parent, const Rect16 &rect)
    : WindowMenu(parent, rect) {
    BindContainer(container);
    // Resolve the heterogeneous container once. Configuration reads/writes are
    // then ordinary runtime loops instead of generating another copy of their
    // logic for every possible tool index.
    stdext::visit_sequence<VirtualToolIndex::count>([&]<size_t ix>() {
        action_items_[ix] = &container.Item<WithConstructorArgs<MI_ActionSelect, ix>>();
    });
}

MultiFilamentChangeConfig MenuMultiFilamentChange::configuration() const {
    MultiFilamentChangeConfig result;
    for (auto tool : VirtualToolIndex::all()) {
        const auto &item = *action_items_[tool.to_raw()];
        result[tool] = item.config();
        result.colors[tool] = item.selected_color();
    }
    return result;
}

void MenuMultiFilamentChange::set_configuration(const MultiFilamentChangeConfig &set) {
    for (auto tool : VirtualToolIndex::all()) {
        action_items_[tool.to_raw()]->set_config(set[tool], set.colors[tool]);
    }
}

void MenuMultiFilamentChange::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {

    case GUI_event_t::CHILD_CLICK: {
        if (carry_out_changes()) {
            Screens::Access()->Close();
        }
        return;
    }

    case GUI_event_t::MEDIA: {
        const MediaState_t media_state = MediaState_t(reinterpret_cast<int>(param));
        if (media_state == MediaState_t::removed || media_state == MediaState_t::error) {
            // USB was removed
            if (close_screen_on_media_disconnect_) {
                Screens::Access()->Close();
                return;
            }
        }
        break;
    }

    default:
        break;
    }

    WindowMenu::windowEvent(sender, event, param);
}

bool MenuMultiFilamentChange::carry_out_changes() {
    const auto config = configuration();
    if (!gui_config_confirm_incompatibilities(config, Response::Cancel)) {
        return false;
    }

    ArrayStringBuilder<MAX_CMD_SIZE> sb;
    multi_filament_change::config_to_gcode(config, sb);
    marlin_client::gcode(sb.str());
    return true;
}

static constexpr const char *header_text = HAS_MMU2() ? N_("FILAMENT CHANGE") : N_("MULTITOOL FILAMENT CHANGE");

ScreenChangeAllFilaments::ScreenChangeAllFilaments()
    : ScreenMenuBase(nullptr, _(header_text), EFooter::On) //
{
    EnableLongHoldScreenAction();
    Screens::Access()->DisableMenuTimeout();
    menu.menu.set_configuration({});
}

ScreenChangeAllFilaments::ScreenChangeAllFilaments(SetupForPrint)
    : ScreenChangeAllFilaments {} {
    menu.menu.set_configuration(multi_filament_change::config_from_current_print_setup());
    menu.menu.close_screen_on_media_disconnect_ = true;
}

ScreenChangeAllFilaments::ScreenChangeAllFilaments(SetupUnloadAll)
    : ScreenChangeAllFilaments {} {

    multi_filament_change::Config config;
    for (auto tool : VirtualToolIndex::all().skip_all_disabled()) {
        config[tool].action = multi_filament_change::Action::unload;
    }
    menu.menu.set_configuration(config);

    // Preselect apply-changes, all should be clear
    menu.menu.move_focus_to_index(menu.menu.container.GetVisibleIndex(menu.menu.container.Item<MI_ApplyChanges>()));
}


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

using namespace multi_filament_change;

namespace {

class MI_PRELOAD_MANUFACTURER final : public IWindowMenuItem {
public:
    MI_PRELOAD_MANUFACTURER(MI_ActionSelect *owner, std::optional<uint8_t> id, std::string_view name)
        : IWindowMenuItem(string_view_utf8::MakeRAM(name.data())), owner_(owner), id_(id) {}
protected:
    void click(IWindowMenu &) override { owner_->set_selected_manufacturer(id_); Screens::Access()->Close(); Screens::Access()->Close(); }
private:
    MI_ActionSelect *owner_;
    std::optional<uint8_t> id_;
};

class WindowMenuPreloadManufacturer final : public WindowMenuVirtual {
public:
    WindowMenuPreloadManufacturer(window_t *parent, Rect16 rect) : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::no) {}
    void set_owner(MI_ActionSelect *owner) { owner_ = owner; custom_count_ = 0; for (size_t i = 0; i < filament_manufacturer::custom_slot_count; ++i) custom_count_ += filament_manufacturer::custom(i).has_value(); setup_items(); }
    int item_count() const override { return 1 + filament_manufacturer::presets().size() + custom_count_; }
protected:
    void setup_item(ItemVariant &variant, int index) override {
        if (index == 0) { variant.emplace<MI_PRELOAD_MANUFACTURER>(owner_, std::nullopt, std::string_view("None")); return; }
        size_t requested = static_cast<size_t>(index - 1);
        if (requested < filament_manufacturer::presets().size()) { variant.emplace<MI_PRELOAD_MANUFACTURER>(owner_, static_cast<uint8_t>(requested + 1), std::string_view(filament_manufacturer::presets()[requested])); return; }
        requested -= filament_manufacturer::presets().size();
        for (size_t slot = 0; slot < filament_manufacturer::custom_slot_count; ++slot) if (const auto profile = filament_manufacturer::custom(slot); profile && requested-- == 0) { variant.emplace<MI_PRELOAD_MANUFACTURER>(owner_, profile->id, profile->name_view()); return; }
    }
private:
    MI_ActionSelect *owner_ = nullptr;
    size_t custom_count_ = 0;
};

class ScreenPreloadManufacturer final : public ScreenMenuBase<WindowMenuPreloadManufacturer> {
public:
    explicit ScreenPreloadManufacturer(MI_ActionSelect *owner) : ScreenMenuBase(nullptr, _("SELECT MANUFACTURER"), EFooter::On) { menu.menu.set_owner(owner); }
};

class MI_PRELOAD_COLOR final : public IWindowMenuItem {
public:
    MI_PRELOAD_COLOR(MI_ActionSelect *owner, std::optional<Color> color, std::string_view name)
        : IWindowMenuItem(string_view_utf8::MakeRAM(name.data()), color ? filament_color_gui::swatch_extension_width : Rect16::Width_t { 0 })
        , owner_(owner)
        , color_(color) {}

protected:
    void printExtension(Rect16 extension_rect, Color, Color color_back, ropfn) const override {
        if (color_) filament_color_gui::draw_swatch(extension_rect, *color_, color_back);
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
        for (size_t i = 0; i < filament_color::custom_slot_count; ++i) custom_count_ += filament_color::custom(i).has_value();
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
    : MenuItemSelectMenu({}) {
    const auto tool = VirtualToolIndex::from_raw(tool_ix);
    has_filament_loaded = (config_store().get_filament_type(tool) != FilamentType::none);
    set_is_hidden(!tool.is_enabled());
    SetLabel(tool.display_name(label_params));
}

MI_ActionSelect::MI_ActionSelect(SetAllToMode)
    : MenuItemSelectMenu(_("Set All To"))
    , set_all_to_mode { true } {
    set_behavior(Behavior::select_only);

    // Necessary to generate filament list
    set_config({});
}

void MI_ActionSelect::set_config(const ConfigItem &set) {
    // By using enforce_first_item, we make sure the target filament is in the list (it might be hidden otherwise) and that it's on the first place (which is a welcome bonus)
    generate_filament_list(filament_list, { .enforce_first_item = set.new_filament });
    index_mapping.set_section_size<Action::change>(filament_list.size());

    color = set.color;
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
        .color = color,
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
    if (set_all_to_mode) {
        auto &menu = static_cast<MenuMultiFilamentChange &>(args.menu);
        auto new_config = menu.configuration();
        const auto new_config_item = this->config(args.new_index);

        for (auto tool : VirtualToolIndex::all().skip_all_disabled()) {
            auto &config_item = new_config[tool];

            // Keep the color
            const auto orig_color = config_item.color;
            const auto orig_manufacturer = config_item.manufacturer;
            config_item = new_config_item;
            config_item.color = orig_color;
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
}

MultiFilamentChangeConfig MenuMultiFilamentChange::configuration() const {
    return [&]<size_t... ix>(std::index_sequence<ix...>) {
        return MultiFilamentChangeConfig {
            ConfigItem { container.Item<WithConstructorArgs<MI_ActionSelect, ix>>().config() }...
        };
    }(std::make_index_sequence<VirtualToolIndex::count>());
}

void MenuMultiFilamentChange::set_configuration(const MultiFilamentChangeConfig &set) {
    // Set the correct indexes for the actions
    stdext::visit_sequence<VirtualToolIndex::count>([&]<size_t ix>() {
        container.Item<WithConstructorArgs<MI_ActionSelect, ix>>().set_config(set[VirtualToolIndex::from_raw(ix)]);
    });
}

void MenuMultiFilamentChange::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {

    case GUI_event_t::CHILD_CLICK: {
        carry_out_changes();
        Screens::Access()->Close();
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

void MenuMultiFilamentChange::carry_out_changes() {
    ArrayStringBuilder<MAX_CMD_SIZE> sb;
    multi_filament_change::config_to_gcode(configuration(), sb);
    marlin_client::gcode(sb.str());
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

#pragma once

#include <meta_utils.hpp>

#include <i_window_menu_item.hpp>
#include <WindowMenuItems.hpp>
#include <filament_list.hpp>
#include <screen_menu.hpp>
#include <window_menu_virtual.hpp>
#include <tool_index.hpp>

class MI_LOADED_FILAMENT : public IWindowMenuItem {

public:
    enum class DisplayFormat {
        /// On multi-slot printer: "Loaded filaments"
        /// On single-slot printer: "Loaded filament (PLA)"
        auto_submenu,

        /// "Slot N (PLA)"
        slot_number,
    };

public:
    MI_LOADED_FILAMENT(DisplayFormat display_format = DisplayFormat::auto_submenu, uint8_t tool = 0);

protected:
    virtual void click(IWindowMenu &) override;
    virtual void Loop() override;

private:
    std::array<char, 48> label_buffer_;
    VirtualToolIndex tool_;
    FilamentType filament_type_;
    DisplayFormat display_format_ : 2;
    bool should_open_submenu_ : 1;
};

template <typename>
struct ScreenLoadedFilaments_ {};

template <size_t... i>
struct ScreenLoadedFilaments_<std::index_sequence<i...>> {
    using T = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, WithConstructorArgs<MI_LOADED_FILAMENT, MI_LOADED_FILAMENT::DisplayFormat::slot_number, i>...>;
};

class ScreenLoadedFilaments : public ScreenLoadedFilaments_<std::make_index_sequence<VirtualToolIndex::count>>::T {
public:
    ScreenLoadedFilaments();
};

namespace screen_loaded_filament_assignment {

class MI_ASSIGN_LOADED_FILAMENT final : public IWindowMenuItem {
public:
    MI_ASSIGN_LOADED_FILAMENT(VirtualToolIndex tool, FilamentType filament_type);

protected:
    void click(IWindowMenu &) override;

private:
    VirtualToolIndex tool_;
    FilamentType filament_type_;
    FilamentTypeParameters::Name filament_name_;
};

class WindowMenuAssignLoadedFilament final : public WindowMenuVirtual {
public:
    WindowMenuAssignLoadedFilament(window_t *parent, Rect16 rect);

    void set_tool(VirtualToolIndex tool);

    int item_count() const final;

protected:
    void setup_item(ItemVariant &variant, int index) final;

private:
    VirtualToolIndex tool_;
    FilamentList filament_list_;
};

} // namespace screen_loaded_filament_assignment

class ScreenAssignLoadedFilament final : public ScreenMenuBase<screen_loaded_filament_assignment::WindowMenuAssignLoadedFilament> {
public:
    ScreenAssignLoadedFilament(uint8_t tool);
};

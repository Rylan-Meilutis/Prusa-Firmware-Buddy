#pragma once

#include <meta_utils.hpp>

#include <i_window_menu_item.hpp>
#include <WindowMenuItems.hpp>
#include <filament_list.hpp>
#include <screen_menu.hpp>
#include <window_menu_virtual.hpp>
#include <filament_color.hpp>

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

private:
    DisplayFormat display_format_;
    uint8_t tool_;
    bool should_open_submenu_;
    FilamentType filament_type_;

    std::array<char, 48> label_buffer_;
};

template <typename>
struct ScreenLoadedFilaments_ {};

template <size_t... i>
struct ScreenLoadedFilaments_<std::index_sequence<i...>> {
    using T = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, WithConstructorArgs<MI_LOADED_FILAMENT, MI_LOADED_FILAMENT::DisplayFormat::slot_number, i>...>;
};

class ScreenLoadedFilaments : public ScreenLoadedFilaments_<std::make_index_sequence<EXTRUDERS>>::T {
public:
    ScreenLoadedFilaments();
};

namespace screen_loaded_filament_assignment {

class MI_LOADED_COLOR_ACTION final : public IWindowMenuItem {
public:
    explicit MI_LOADED_COLOR_ACTION(uint8_t tool);
protected:
    void click(IWindowMenu &) override;
private:
    uint8_t tool_;
};

class MI_ASSIGN_LOADED_FILAMENT final : public IWindowMenuItem {
public:
    MI_ASSIGN_LOADED_FILAMENT(uint8_t tool, FilamentType filament_type);

protected:
    void click(IWindowMenu &) override;

private:
    uint8_t tool_;
    FilamentType filament_type_;
    FilamentTypeParameters::Name filament_name_;
};

class WindowMenuAssignLoadedFilament final : public WindowMenuVirtual<MI_RETURN, MI_LOADED_COLOR_ACTION, MI_ASSIGN_LOADED_FILAMENT> {
public:
    WindowMenuAssignLoadedFilament(window_t *parent, Rect16 rect);

    void set_tool(uint8_t tool);

    int item_count() const final;

protected:
    void setup_item(ItemVariant &variant, int index) final;

private:
    uint8_t tool_;
    FilamentList filament_list_;
};

} // namespace screen_loaded_filament_assignment

namespace screen_loaded_color_assignment {
class MI_ADD_CUSTOM_COLOR final : public IWindowMenuItem {
public:
    MI_ADD_CUSTOM_COLOR();
protected:
    void click(IWindowMenu &) override;
};
class MI_ASSIGN_LOADED_COLOR final : public IWindowMenuItem {
public:
    MI_ASSIGN_LOADED_COLOR(uint8_t tool, std::optional<Color> color, std::string_view name);
protected:
    void click(IWindowMenu &) override;
private:
    uint8_t tool_;
    std::optional<Color> color_;
    std::array<char, 32> label_ {};
};

class WindowMenuAssignLoadedColor final : public WindowMenuVirtual<MI_RETURN, MI_ADD_CUSTOM_COLOR, MI_ASSIGN_LOADED_COLOR> {
public:
    WindowMenuAssignLoadedColor(window_t *parent, Rect16 rect);
    void set_tool(uint8_t tool);
    int item_count() const final;
protected:
    void setup_item(ItemVariant &variant, int index) final;
private:
    uint8_t tool_;
    size_t custom_count_ = 0;
};
}

class ScreenAssignLoadedColor final : public ScreenMenuBase<screen_loaded_color_assignment::WindowMenuAssignLoadedColor> {
public:
    ScreenAssignLoadedColor(uint8_t tool);
};

class ScreenAssignLoadedFilament final : public ScreenMenuBase<screen_loaded_filament_assignment::WindowMenuAssignLoadedFilament> {
public:
    ScreenAssignLoadedFilament(uint8_t tool);
};

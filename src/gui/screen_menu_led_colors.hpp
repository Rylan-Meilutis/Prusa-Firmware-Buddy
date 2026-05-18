#pragma once

#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"

namespace ui_theme_menu {
bool load_usb_theme_file(const char *path);
}

class MI_UI_THEME_PRESET_PRUSA : public IWindowMenuItem {
public:
    MI_UI_THEME_PRESET_PRUSA();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_PRESET_DEFAULT : public IWindowMenuItem {
public:
    MI_UI_THEME_PRESET_DEFAULT();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_PRESET_BLUE : public IWindowMenuItem {
public:
    MI_UI_THEME_PRESET_BLUE();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_PRESET_GRAPHITE : public IWindowMenuItem {
public:
    MI_UI_THEME_PRESET_GRAPHITE();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_PRESET_FOREST : public IWindowMenuItem {
public:
    MI_UI_THEME_PRESET_FOREST();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_CUSTOM_COLORS : public IWindowMenuItem {
public:
    MI_UI_THEME_CUSTOM_COLORS();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_IMPORTED_COLORS : public IWindowMenuItem {
public:
    MI_UI_THEME_IMPORTED_COLORS();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_LOAD_USB : public IWindowMenuItem {
public:
    MI_UI_THEME_LOAD_USB();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_APPLY_IMPORTED : public IWindowMenuItem {
public:
    MI_UI_THEME_APPLY_IMPORTED();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_PRIMARY_COLOR : public IWindowMenuItem {
public:
    MI_UI_THEME_PRIMARY_COLOR();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_PROGRESS_COLOR : public IWindowMenuItem {
public:
    MI_UI_THEME_PROGRESS_COLOR();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_WARNING_COLOR : public IWindowMenuItem {
public:
    MI_UI_THEME_WARNING_COLOR();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_ERROR_COLOR : public IWindowMenuItem {
public:
    MI_UI_THEME_ERROR_COLOR();
    void click(IWindowMenu &) override;
};

class MI_UI_THEME_IMAGE_COLOR : public IWindowMenuItem {
public:
    MI_UI_THEME_IMAGE_COLOR();
    void click(IWindowMenu &) override;
};

class MI_USB_THEME_PRIMARY_COLOR : public IWindowMenuItem {
public:
    MI_USB_THEME_PRIMARY_COLOR();
    void click(IWindowMenu &) override;
};

class MI_USB_THEME_PROGRESS_COLOR : public IWindowMenuItem {
public:
    MI_USB_THEME_PROGRESS_COLOR();
    void click(IWindowMenu &) override;
};

class MI_USB_THEME_WARNING_COLOR : public IWindowMenuItem {
public:
    MI_USB_THEME_WARNING_COLOR();
    void click(IWindowMenu &) override;
};

class MI_USB_THEME_ERROR_COLOR : public IWindowMenuItem {
public:
    MI_USB_THEME_ERROR_COLOR();
    void click(IWindowMenu &) override;
};

class MI_USB_THEME_IMAGE_COLOR : public IWindowMenuItem {
public:
    MI_USB_THEME_IMAGE_COLOR();
    void click(IWindowMenu &) override;
};

using ScreenMenuUiThemeColors__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_UI_THEME_PRESET_PRUSA, MI_UI_THEME_PRESET_DEFAULT, MI_UI_THEME_PRESET_BLUE, MI_UI_THEME_PRESET_GRAPHITE, MI_UI_THEME_PRESET_FOREST, MI_UI_THEME_CUSTOM_COLORS, MI_UI_THEME_IMPORTED_COLORS, MI_UI_THEME_LOAD_USB>;

class ScreenMenuUiThemeColors final : public ScreenMenuUiThemeColors__ {
public:
    ScreenMenuUiThemeColors();
};

using ScreenMenuUiThemeCustom__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_UI_THEME_PRIMARY_COLOR, MI_UI_THEME_PROGRESS_COLOR, MI_UI_THEME_WARNING_COLOR, MI_UI_THEME_ERROR_COLOR, MI_UI_THEME_IMAGE_COLOR>;

class ScreenMenuUiThemeCustom final : public ScreenMenuUiThemeCustom__ {
public:
    ScreenMenuUiThemeCustom();
};

using ScreenMenuUiThemeImported__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_UI_THEME_APPLY_IMPORTED, MI_USB_THEME_PRIMARY_COLOR, MI_USB_THEME_PROGRESS_COLOR, MI_USB_THEME_WARNING_COLOR, MI_USB_THEME_ERROR_COLOR, MI_USB_THEME_IMAGE_COLOR>;

class ScreenMenuUiThemeImported final : public ScreenMenuUiThemeImported__ {
public:
    ScreenMenuUiThemeImported();
};

class MI_STATUS_LED_IDLE_COLOR : public IWindowMenuItem {
public:
    MI_STATUS_LED_IDLE_COLOR();
    void click(IWindowMenu &) override;
};

class MI_STATUS_LED_PRINTING_COLOR : public IWindowMenuItem {
public:
    MI_STATUS_LED_PRINTING_COLOR();
    void click(IWindowMenu &) override;
};

class MI_STATUS_LED_FINISHED_COLOR : public IWindowMenuItem {
public:
    MI_STATUS_LED_FINISHED_COLOR();
    void click(IWindowMenu &) override;
};

class MI_STATUS_LED_WARNING_COLOR : public IWindowMenuItem {
public:
    MI_STATUS_LED_WARNING_COLOR();
    void click(IWindowMenu &) override;
};

class MI_STATUS_LED_ERROR_COLOR : public IWindowMenuItem {
public:
    MI_STATUS_LED_ERROR_COLOR();
    void click(IWindowMenu &) override;
};

using ScreenMenuStatusLedColors__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_STATUS_LED_IDLE_COLOR, MI_STATUS_LED_PRINTING_COLOR, MI_STATUS_LED_FINISHED_COLOR, MI_STATUS_LED_WARNING_COLOR, MI_STATUS_LED_ERROR_COLOR>;

class ScreenMenuStatusLedColors final : public ScreenMenuStatusLedColors__ {
public:
    ScreenMenuStatusLedColors();
};

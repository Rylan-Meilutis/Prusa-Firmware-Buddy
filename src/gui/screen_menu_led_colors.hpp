#pragma once

#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"

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

using ScreenMenuUiThemeColors__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_UI_THEME_PRIMARY_COLOR, MI_UI_THEME_PROGRESS_COLOR, MI_UI_THEME_WARNING_COLOR, MI_UI_THEME_ERROR_COLOR>;

class ScreenMenuUiThemeColors final : public ScreenMenuUiThemeColors__ {
public:
    ScreenMenuUiThemeColors();
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

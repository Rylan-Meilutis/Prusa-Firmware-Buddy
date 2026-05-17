#pragma once

#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"

class MI_PRINTER_LOCK_ENABLE : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_PRINTER_LOCK_ENABLE();
    void OnChange(size_t old_index) override;
};

class MI_PRINTER_LOCK_PIN : public IWindowMenuItem {
public:
    MI_PRINTER_LOCK_PIN();
    void click(IWindowMenu &) override;
};

class MI_PRINTER_LOCK_TIMEOUT : public WiSpin {
public:
    MI_PRINTER_LOCK_TIMEOUT();
    void OnClick() override;
};

class MI_PRINTER_LOCK_SERIAL : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_PRINTER_LOCK_SERIAL();
    void OnChange(size_t old_index) override;
};

using ScreenMenuLockSettings__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_PRINTER_LOCK_ENABLE, MI_PRINTER_LOCK_PIN, MI_PRINTER_LOCK_TIMEOUT, MI_PRINTER_LOCK_SERIAL>;

class ScreenMenuLockSettings final : public ScreenMenuLockSettings__ {
public:
    ScreenMenuLockSettings();
};

#pragma once

#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"

class MI_SERIAL_PRINT_SCREEN_ENABLE : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_SERIAL_PRINT_SCREEN_ENABLE();
    void OnChange(size_t old_index) override;
};

class MI_SERIAL_PRINT_LEGACY_UI : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_SERIAL_PRINT_LEGACY_UI();
    void OnChange(size_t old_index) override;
};

class MI_SERIAL_PRINT_AUTO_START : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_SERIAL_PRINT_AUTO_START();
    void OnChange(size_t old_index) override;
};

class MI_SERIAL_PRINT_TIMEOUT : public WiSpin {
public:
    MI_SERIAL_PRINT_TIMEOUT();
    void OnClick() override;
};

using ScreenMenuSerialPrinting__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_SERIAL_PRINT_SCREEN_ENABLE, MI_SERIAL_PRINT_LEGACY_UI, MI_SERIAL_PRINT_AUTO_START, MI_SERIAL_PRINT_TIMEOUT>;

class ScreenMenuSerialPrinting final : public ScreenMenuSerialPrinting__ {
public:
    ScreenMenuSerialPrinting();
};

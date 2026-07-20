#pragma once

#include <screen_menu.hpp>
#include <WindowMenuItems.hpp>
#include <WindowMenuSpin.hpp>
#include <WindowMenuSwitch.hpp>
#include <printers.h>

#define HAS_PA_CALIBRATION_UI() (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX())

class MI_PA_TOOL final : public MenuItemSwitch {
public:
    MI_PA_TOOL();
protected:
    void OnChange(size_t old_index) override;
    bool on_item_selected(const OnItemSelectedArgs &args) override;
};

class MI_PA_SEQUENTIAL final : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_PA_SEQUENTIAL();
protected:
    void OnChange(size_t old_index) override;
};

class MI_PA_TEMPERATURE final : public WiSpin {
public:
    MI_PA_TEMPERATURE();
protected:
    void OnClick() override;
};

class MI_PA_RUN final : public IWindowMenuItem {
public:
    MI_PA_RUN();
protected:
    void click(IWindowMenu &) override;
};

using ScreenMenuPACalibration_ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    MI_PA_TOOL, MI_PA_SEQUENTIAL, MI_PA_TEMPERATURE, MI_PA_RUN>;

class ScreenMenuPACalibration final : public ScreenMenuPACalibration_ {
public:
    ScreenMenuPACalibration();
};

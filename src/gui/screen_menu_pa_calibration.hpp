#pragma once

#include <screen_menu.hpp>
#include <WindowMenuItems.hpp>
#include <WindowMenuSpin.hpp>
#include <printers.h>

#define HAS_PA_CALIBRATION_UI() (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX())

class MI_PA_TOOL_RUN : public IWindowMenuItem {
public:
    explicit MI_PA_TOOL_RUN(uint8_t tool);
protected:
    void click(IWindowMenu &) override;
private:
    uint8_t tool_;
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
    WithConstructorArgs<MI_PA_TOOL_RUN, 0>, WithConstructorArgs<MI_PA_TOOL_RUN, 1>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 2>, WithConstructorArgs<MI_PA_TOOL_RUN, 3>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 4>, WithConstructorArgs<MI_PA_TOOL_RUN, 5>,
    MI_PA_TEMPERATURE, MI_PA_RUN>;

class ScreenMenuPACalibration final : public ScreenMenuPACalibration_ {
public:
    ScreenMenuPACalibration();
};

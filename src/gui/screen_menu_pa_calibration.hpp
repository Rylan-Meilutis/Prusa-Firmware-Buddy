#pragma once

#include <screen_menu.hpp>
#include <WindowMenuItems.hpp>
#include <WindowMenuSpin.hpp>
#include <printers.h>

#define HAS_PA_CALIBRATION_UI() (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_iX())

class MI_PA_TOOL_RUN : public WI_ICON_SWITCH_OFF_ON_t {
public:
    explicit MI_PA_TOOL_RUN(uint8_t tool);
protected:
    void OnChange(size_t old_index) override;
private:
    uint8_t tool_;
};

class MI_PA_TEMPERATURE : public WiSpin {
public:
    explicit MI_PA_TEMPERATURE(uint8_t tool);
protected:
    void OnClick() override;
private:
    uint8_t tool_;
};

class MI_PA_RUN final : public IWindowMenuItem {
public:
    MI_PA_RUN();
protected:
    void click(IWindowMenu &) override;
};

using ScreenMenuPACalibration_ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    WithConstructorArgs<MI_PA_TOOL_RUN, 0>, WithConstructorArgs<MI_PA_TEMPERATURE, 0>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 1>, WithConstructorArgs<MI_PA_TEMPERATURE, 1>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 2>, WithConstructorArgs<MI_PA_TEMPERATURE, 2>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 3>, WithConstructorArgs<MI_PA_TEMPERATURE, 3>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 4>, WithConstructorArgs<MI_PA_TEMPERATURE, 4>,
    WithConstructorArgs<MI_PA_TOOL_RUN, 5>, WithConstructorArgs<MI_PA_TEMPERATURE, 5>,
    MI_PA_RUN>;

class ScreenMenuPACalibration final : public ScreenMenuPACalibration_ {
public:
    ScreenMenuPACalibration();
};

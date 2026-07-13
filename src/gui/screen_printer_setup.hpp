#pragma once

#include <window_header.hpp>
#include <window_menu_adv.hpp>
#include <window_text.hpp>
#include <window_menu.hpp>
#include <MItem_hardware.hpp>
#include <WinMenuContainer.hpp>
#include <screen_menu.hpp>
#include <common/extended_printer_type.hpp>
#include <common/printer_variant/printer_variant.hpp>
#include <gui/screen/screen_menu_printer_setup_hardware.hpp>

#include <MItem_menus.hpp>
#include <option/has_mmu2.h>
#if HAS_MMU2()
    #include <MItem_mmu.hpp>
#endif

#include <option/xbuddy_extension_variant.h>
#if XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
    #include <gui/menu_item/specific/menu_items_xbuddy_extension.hpp>
#endif

namespace screen_printer_setup_private {

class MI_DONE : public IWindowMenuItem {

public:
    MI_DONE();

protected:
    void click(IWindowMenu &menu) override;
};

using MI_PRINTER_SETUP_HARDWARE = MI_SCREEN<N_("Hardware"), ScreenMenuPrinterSetupHardware>;

using ScreenBase
    = ScreenMenu<EFooter::Off,
        MI_EXTENDED_PRINTER_TYPE, //< Show always, for non-extended models, there is a non-changeable WiInfo
#if HAS_PRINTER_VARIANT()
        MI_PRINTER_VARIANT,
#endif
        MI_TOOLHEAD_SETTINGS,
#if XBUDDY_EXTENSION_VARIANT_IS_STANDARD()
        MI_CAM_USB_PWR,
#endif
        MI_PRINTER_SETUP_HARDWARE,
        MI_DONE>;

class ScreenPrinterSetup : public ScreenBase {
public:
    ScreenPrinterSetup();

    [[nodiscard]] static bool should_show();
};

} // namespace screen_printer_setup_private

using screen_printer_setup_private::ScreenPrinterSetup;

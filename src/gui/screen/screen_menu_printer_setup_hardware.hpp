/// @file
#pragma once

#include <MItem_hardware.hpp>
#include <MItem_tools.hpp>
#include <screen_menu.hpp>
#include <option/has_expansion_joints_gen_2.h>
#include <option/has_15gt_belts.h>
#include <option/has_nozzle_cleaner_lite.h>
#include <option/has_chamber_vents.h>

#include <option/has_chamber_filtration_api.h>
#if HAS_CHAMBER_FILTRATION_API()
    #include <gui/menu_item/specific/menu_items_chamber_filtration.hpp>
#endif

// Home for printer-setup (very first boot) hardware settings
using ScreenMenuPrinterSetupHardwareBase = ScreenMenu<EFooter::On,
    MI_RETURN,
#if HAS_CHAMBER_FILTRATION_API()
    // At least for C1, the filter addon is considered a hardware option, because it also affects the function of the cooling fans
    // BFW-6719
    MI_CHAMBER_FILTRATION_BACKEND,
#endif
#if HAS_CHAMBER_VENTS()
    MI_SWITCH_VENT_MECHANISM,
#endif
#if HAS_EXPANSION_JOINTS_GEN_2()
    MI_EXPANSION_JOINTS_GEN_2,
#endif
#if HAS_15GT_BELTS()
    MI_BELTS_15GT,
#endif
#if HAS_NOZZLE_CLEANER_LITE()
    MI_NOZZLE_CLEANER_LITE,
#endif
    MI_ALWAYS_HIDDEN>;

class ScreenMenuPrinterSetupHardware : public ScreenMenuPrinterSetupHardwareBase {
public:
    ScreenMenuPrinterSetupHardware();
};

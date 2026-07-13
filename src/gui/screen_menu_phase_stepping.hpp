/// @file
#pragma once

#include <common/meta_utils.hpp>
#include <common/str_utils.hpp>
#include <gui/basic_screen_menu.hpp>
#include <gui/menu_item/menu_item_gcode_action.hpp>
#include <lang/i18n.h>
#include <option/has_phase_stepping.h>

static_assert(HAS_PHASE_STEPPING(), "Do not #include me if you are not using me");

using MI_PHASE_STEPPING_CALIBRATION = WithConstructorArgs<
    MenuItemGcodeAction,
    N_("Calibration"), "M1977"_tstr>;

using MI_PHASE_STEPPING_RESTORE_DEFAULTS = WithConstructorArgs<
    MenuItemGcodeAction,
    N_("Restore Defaults"), "M1977 D"_tstr>;

using ScreenMenuPhaseSteppingBase = BasicScreenMenu<
    MI_PHASE_STEPPING_CALIBRATION,
    MI_PHASE_STEPPING_RESTORE_DEFAULTS>;

class ScreenMenuPhaseStepping final : public ScreenMenuPhaseSteppingBase {
public:
    ScreenMenuPhaseStepping();
};

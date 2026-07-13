/// @file
#include <gui/screen_menu_phase_stepping.hpp>

#include <img_resources.hpp>
#include <lang/i18n.h>

ScreenMenuPhaseStepping::ScreenMenuPhaseStepping()
    : ScreenMenuPhaseSteppingBase {
        _("PHASE STEPPING"),
        &img::settings_16x16,
    } {}

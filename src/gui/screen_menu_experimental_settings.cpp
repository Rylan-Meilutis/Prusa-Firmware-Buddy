/// @file
#include "screen_menu_experimental_settings.hpp"

#include "WindowMenuSpin.hpp"
#include "ScreenHandler.hpp"
#include "window_msgbox.hpp"
#include "string.h" // memcmp
#include "MItem_experimental_tools.hpp"
#include <common/sys.hpp>
#include <config_store/store_instance.hpp>
#include <config_store/store_c_api.h>

void ScreenMenuExperimentalSettings::clicked_return() {
    ExperimentalSettingsValues current(*this); // ctor will handle load of values
    // unchanged
    if (current == initial) {
        Screens::Access()->Close();
        return;
    }

    switch (MsgBoxQuestion(_(save_and_reboot), Responses_YesNoCancel)) {
    case Response::Yes:
        Item<MI_Z_AXIS_LEN>().Store();

#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
        Item<MI_STEPS_PER_UNIT_X>().Store(Item<MI_DIRECTION_X>());
        Item<MI_STEPS_PER_UNIT_Y>().Store(Item<MI_DIRECTION_Y>());
        Item<MI_STEPS_PER_UNIT_Z>().Store();
        Item<MI_DIRECTION_Z>().Store();
#endif

        Item<MI_STEPS_PER_UNIT_E>().Store();
        Item<MI_DIRECTION_E>().Store();

#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
        Item<MI_CURRENT_X>().Store();
        Item<MI_CURRENT_Y>().Store();
        Item<MI_CURRENT_Z>().Store();
        Item<MI_CURRENT_E>().Store();
#endif

#if HAS_LOADCELL()
        Item<MI_LOADCELL_SCALE>().Store();
#endif // HAS_LOADCELL()

        sys_reset();
    case Response::No:
        Screens::Access()->Close();
        return;
    default:
        return; // do nothing
    }
}

ScreenMenuExperimentalSettings::ScreenMenuExperimentalSettings()
    : ScreenMenuExperimentalSettings__(_(label))
    , initial(*this) {}

void ScreenMenuExperimentalSettings::windowEvent(window_t *sender, GUI_event_t ev, void *param) {
    if (ev != GUI_event_t::CHILD_CLICK) {
        ScreenMenu::windowEvent(sender, ev, param);
        return;
    }

    switch (ClickCommand(intptr_t(param))) {
    case ClickCommand::Return:
        clicked_return();
        break;

    case ClickCommand::Reset_Z:
        Item<MI_Z_AXIS_LEN>().SetVal(DEFAULT_Z_MAX_POS);
        Invalidate();
        break;

    case ClickCommand::Reset_steps:
#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
        Item<MI_STEPS_PER_UNIT_X>().set_value(std::nullopt);
        Item<MI_DIRECTION_X>().set_current_item(0);
        Item<MI_STEPS_PER_UNIT_Y>().set_value(std::nullopt);
        Item<MI_DIRECTION_Y>().set_current_item(0);
        Item<MI_STEPS_PER_UNIT_Z>().SetVal(std::abs(config_store().axis_steps_per_unit_z.default_val));
        Item<MI_DIRECTION_Z>().set_current_item(0);
#endif
        Item<MI_STEPS_PER_UNIT_E>().SetVal(std::abs(config_store().axis_steps_per_unit_e0.default_val));
        Item<MI_DIRECTION_E>().set_current_item(0);
        Invalidate();
        break;

    case ClickCommand::Reset_directions:
#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
        // set index to Prusa
        Item<MI_DIRECTION_X>().set_current_item(0);
        Item<MI_DIRECTION_Y>().set_current_item(0);
        Item<MI_DIRECTION_Z>().set_current_item(0);
#endif
        Item<MI_DIRECTION_E>().set_current_item(0);
        Invalidate();
        break;

    case ClickCommand::Reset_currents:
#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
        // 0 is valid for X and Y axis, means to use default values
        Item<MI_CURRENT_X>().SetVal(config_store().axis_rms_current_ma_X_.default_val);
        Item<MI_CURRENT_Y>().SetVal(config_store().axis_rms_current_ma_Y_.default_val);
        Item<MI_CURRENT_Z>().SetVal(config_store().axis_rms_current_ma_Z_.default_val);
        Item<MI_CURRENT_E>().SetVal(config_store().axis_rms_current_ma_E0_.default_val);
#endif
        Invalidate();
        break;
    }
}

ExperimentalSettingsValues::ExperimentalSettingsValues(ScreenMenuExperimentalSettings__ &parent)
    : z_len(static_cast<int32_t>(parent.Item<MI_Z_AXIS_LEN>().GetVal()))
#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
    , steps_per_unit_x(parent.Item<MI_STEPS_PER_UNIT_X>().value_to_store(parent.Item<MI_DIRECTION_X>()))
    , steps_per_unit_y(parent.Item<MI_STEPS_PER_UNIT_Y>().value_to_store(parent.Item<MI_DIRECTION_Y>()))
    , steps_per_unit_z(parent.Item<MI_STEPS_PER_UNIT_Z>().GetVal() * ((parent.Item<MI_DIRECTION_Z>().get_index() == 1) ? -1 : 1))
#endif
    , steps_per_unit_e(parent.Item<MI_STEPS_PER_UNIT_E>().GetVal() * ((parent.Item<MI_DIRECTION_E>().get_index() == 1) ? -1 : 1))
#if HAS_EXTRA_EXPERIMENTAL_SETTINGS()
    , rms_current_ma_x(static_cast<int32_t>(parent.Item<MI_CURRENT_X>().GetVal()))
    , rms_current_ma_y(static_cast<int32_t>(parent.Item<MI_CURRENT_Y>().GetVal()))
    , rms_current_ma_z(static_cast<int32_t>(parent.Item<MI_CURRENT_Z>().GetVal()))
    , rms_current_ma_e(static_cast<int32_t>(parent.Item<MI_CURRENT_E>().GetVal()))
#endif
#if HAS_LOADCELL()
    , loadcell_scale(parent.Item<MI_LOADCELL_SCALE>().value())
#endif
{
}

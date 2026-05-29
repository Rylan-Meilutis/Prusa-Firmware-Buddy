#include "screen_menu_pid.hpp"

#include "ScreenHandler.hpp"
#include "config_store/store_instance.hpp"
#include "module/temperature.h"
#include <numeric_input_config.hpp>

namespace {

static constexpr NumericInputConfig pid_value_config {
    .min_value = 0,
    .max_value = 999,
    .step = 0.1f,
    .max_decimal_places = 2,
};

float persisted_pid_value(PidHeater heater, PidTerm term) {
    switch (heater) {
    case PidHeater::hotend:
        switch (term) {
        case PidTerm::p:
            return config_store().pid_nozzle_p.get();
        case PidTerm::i:
            return unscalePID_i(config_store().pid_nozzle_i.get());
        case PidTerm::d:
            return unscalePID_d(config_store().pid_nozzle_d.get());
        }
        break;
    case PidHeater::bed:
#if ENABLED(PIDTEMPBED)
        switch (term) {
        case PidTerm::p:
            return config_store().pid_bed_p.get();
        case PidTerm::i:
            return unscalePID_i(config_store().pid_bed_i.get());
        case PidTerm::d:
            return unscalePID_d(config_store().pid_bed_d.get());
        }
#endif
        break;
    }

    return 0;
}

void apply_hotend_pid(float p, float i, float d) {
#if ENABLED(PIDTEMP)
    config_store().pid_nozzle_p.set(p);
    config_store().pid_nozzle_i.set(scalePID_i(i));
    config_store().pid_nozzle_d.set(scalePID_d(d));

    for (int8_t e = 0; e < HOTENDS; e++) {
        PID_PARAM(Kp, e) = p;
        PID_PARAM(Ki, e) = scalePID_i(i);
        PID_PARAM(Kd, e) = scalePID_d(d);
    }
    thermalManager.updatePID();
#endif
}

#if ENABLED(PIDTEMPBED)
void apply_bed_pid(float p, float i, float d) {
    config_store().pid_bed_p.set(p);
    config_store().pid_bed_i.set(scalePID_i(i));
    config_store().pid_bed_d.set(scalePID_d(d));

    thermalManager.temp_bed.pid.Kp = p;
    thermalManager.temp_bed.pid.Ki = scalePID_i(i);
    thermalManager.temp_bed.pid.Kd = scalePID_d(d);
}
#endif

void reset_hotend_pid() {
#if ENABLED(PIDTEMP)
    apply_hotend_pid(DEFAULT_Kp, DEFAULT_Ki, DEFAULT_Kd);
#endif
}

#if ENABLED(PIDTEMPBED)
void reset_bed_pid() {
    apply_bed_pid(DEFAULT_bedKp, DEFAULT_bedKi, DEFAULT_bedKd);
}
#endif

void apply_pid_term(PidHeater heater, PidTerm term, float value) {
    float p = persisted_pid_value(heater, PidTerm::p);
    float i = persisted_pid_value(heater, PidTerm::i);
    float d = persisted_pid_value(heater, PidTerm::d);

    switch (term) {
    case PidTerm::p:
        p = value;
        break;
    case PidTerm::i:
        i = value;
        break;
    case PidTerm::d:
        d = value;
        break;
    }

    switch (heater) {
    case PidHeater::hotend:
        apply_hotend_pid(p, i, d);
        break;
    case PidHeater::bed:
#if ENABLED(PIDTEMPBED)
        apply_bed_pid(p, i, d);
#endif
        break;
    }
}

} // namespace

MI_PID_VALUE::MI_PID_VALUE(PidHeater heater, PidTerm term, const char *label)
    : WiSpin(persisted_pid_value(heater, term), pid_value_config, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no)
    , heater_(heater)
    , term_(term) {
}

void MI_PID_VALUE::OnClick() {
    apply_pid_term(heater_, term_, value());
}

void MI_PID_VALUE::reload() {
    SetVal(persisted_pid_value(heater_, term_));
}

MI_PID_HOTEND_P::MI_PID_HOTEND_P()
    : MI_PID_VALUE(PidHeater::hotend, PidTerm::p, N_("Hotend P")) {}

MI_PID_HOTEND_I::MI_PID_HOTEND_I()
    : MI_PID_VALUE(PidHeater::hotend, PidTerm::i, N_("Hotend I")) {}

MI_PID_HOTEND_D::MI_PID_HOTEND_D()
    : MI_PID_VALUE(PidHeater::hotend, PidTerm::d, N_("Hotend D")) {}

MI_PID_RESET_HOTEND::MI_PID_RESET_HOTEND()
    : IWindowMenuItem(_(N_("Reset Hotend PID")), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_PID_RESET_HOTEND::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->WindowEvent(GUI_event_t::CHILD_CLICK, this);
}

#if ENABLED(PIDTEMPBED)
MI_PID_BED_P::MI_PID_BED_P()
    : MI_PID_VALUE(PidHeater::bed, PidTerm::p, N_("Heatbed P")) {}

MI_PID_BED_I::MI_PID_BED_I()
    : MI_PID_VALUE(PidHeater::bed, PidTerm::i, N_("Heatbed I")) {}

MI_PID_BED_D::MI_PID_BED_D()
    : MI_PID_VALUE(PidHeater::bed, PidTerm::d, N_("Heatbed D")) {}

MI_PID_RESET_BED::MI_PID_RESET_BED()
    : IWindowMenuItem(_(N_("Reset Heatbed PID")), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_PID_RESET_BED::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->WindowEvent(GUI_event_t::CHILD_CLICK, this);
}
#endif

ScreenMenuPid::ScreenMenuPid()
    : screen_menu_pid::ScreenBase(_("PID SETTINGS")) {}

void ScreenMenuPid::reload_items() {
#if ENABLED(PIDTEMP)
    Item<MI_PID_HOTEND_P>().reload();
    Item<MI_PID_HOTEND_I>().reload();
    Item<MI_PID_HOTEND_D>().reload();
#endif
#if ENABLED(PIDTEMPBED)
    Item<MI_PID_BED_P>().reload();
    Item<MI_PID_BED_I>().reload();
    Item<MI_PID_BED_D>().reload();
#endif
}

void ScreenMenuPid::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::CHILD_CLICK) {
#if ENABLED(PIDTEMP)
        if (param == &Item<MI_PID_RESET_HOTEND>()) {
            reset_hotend_pid();
            reload_items();
            return;
        }
#endif
#if ENABLED(PIDTEMPBED)
        if (param == &Item<MI_PID_RESET_BED>()) {
            reset_bed_pid();
            reload_items();
            return;
        }
#endif
    }

    screen_menu_pid::ScreenBase::windowEvent(sender, event, param);
}

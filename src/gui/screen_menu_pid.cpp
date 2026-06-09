#include "screen_menu_pid.hpp"

#include "ScreenHandler.hpp"
#include "config_store/store_instance.hpp"
#include "marlin_client.hpp"
#include "module/temperature.h"
#include <pid_autotune_status.hpp>
#include <numeric_input_config.hpp>
#include <window_msgbox.hpp>
#include <cstdio>

namespace {

static constexpr NumericInputConfig pid_value_config {
    .min_value = 0,
    .max_value = 999,
    .step = 0.1f,
    .max_decimal_places = 2,
};

static constexpr int8_t pid_autotune_cycles = 5;
static constexpr int16_t hotend_autotune_temp = 215;
static constexpr int16_t bed_autotune_temp = 60;

constexpr Rect16 status_rect() {
    return Rect16(20, GuiDefaults::HeaderHeight + 28, GuiDefaults::ScreenWidth - 40, 34);
}

constexpr Rect16 temperature_rect() {
    return Rect16(20, status_rect().Bottom() + 14, GuiDefaults::ScreenWidth - 40, 24);
}

constexpr Rect16 cycle_rect() {
    return Rect16(20, temperature_rect().Bottom() + 8, GuiDefaults::ScreenWidth - 40, 24);
}

constexpr Rect16 progress_rect() {
    return Rect16(40, cycle_rect().Bottom() + 18, GuiDefaults::ScreenWidth - 80, 14);
}

pid_autotune_status::Heater status_heater(PidHeater heater) {
    return heater == PidHeater::bed ? pid_autotune_status::Heater::bed : pid_autotune_status::Heater::hotend;
}

const char *heater_title(PidHeater heater) {
    return heater == PidHeater::bed ? N_("HEATBED PID") : N_("HOTEND PID");
}

int16_t autotune_target(PidHeater heater) {
    return heater == PidHeater::bed ? bed_autotune_temp : hotend_autotune_temp;
}

int8_t autotune_extruder(PidHeater heater) {
    return heater == PidHeater::bed ? -1 : 0;
}

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

MI_PID_AUTOTUNE_HOTEND::MI_PID_AUTOTUNE_HOTEND()
    : IWindowMenuItem(_(N_("Tune Hotend PID")), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_PID_AUTOTUNE_HOTEND::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenPidAutotuneHotend>);
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

MI_PID_AUTOTUNE_BED::MI_PID_AUTOTUNE_BED()
    : IWindowMenuItem(_(N_("Tune Heatbed PID")), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_PID_AUTOTUNE_BED::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenPidAutotuneBed>);
}
#endif

MI_PID_HOTEND_MENU::MI_PID_HOTEND_MENU()
    : IWindowMenuItem(_(N_("Hotend")), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_PID_HOTEND_MENU::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuPidHotend>);
}

MI_PID_BED_MENU::MI_PID_BED_MENU()
    : IWindowMenuItem(_(N_("Heatbed")), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {}

void MI_PID_BED_MENU::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuPidBed>);
}

ScreenPidAutotune::ScreenPidAutotune(PidHeater heater)
    : screen_t()
    , heater_(heater)
    , header_(this)
    , status_(this, status_rect(), is_multiline::yes)
    , temperature_(this, temperature_rect(), is_multiline::no)
    , cycle_(this, cycle_rect(), is_multiline::no)
    , progress_(this, progress_rect(), COLOR_BRAND, COLOR_GRAY)
    , radio_(this, GuiDefaults::GetButtonRect(GetRect()), PhaseResponses { Response::Abort, Response::_none, Response::_none, Response::_none }) {
    CaptureNormalWindow(radio_);

    header_.SetText(_(heater_title(heater_)));
    status_.SetAlignment(Align_t::Center());
    temperature_.SetAlignment(Align_t::Center());
    cycle_.SetAlignment(Align_t::Center());

    snprintf(status_text_, sizeof(status_text_), "%s", "Waiting to start");
    status_.SetText(string_view_utf8::MakeRAM(status_text_));
    temperature_.SetText(string_view_utf8::MakeRAM(temperature_text_));
    cycle_.SetText(string_view_utf8::MakeRAM(cycle_text_));
    progress_.set_progress_percent(0);

    start_autotune();
}

void ScreenPidAutotune::start_autotune() {
    if (command_sent_) {
        return;
    }

    command_sent_ = true;
    pid_autotune_status::clear();
    marlin_client::gcode_printf(
        "M303 E%i S%i C%i",
        static_cast<int>(autotune_extruder(heater_)),
        static_cast<int>(autotune_target(heater_)),
        static_cast<int>(pid_autotune_cycles));
}

void ScreenPidAutotune::update_status() {
    const auto status = pid_autotune_status::snapshot();

    if (status.heater != status_heater(heater_) && !status.finished) {
        snprintf(status_text_, sizeof(status_text_), "%s", "Waiting to start");
        snprintf(temperature_text_, sizeof(temperature_text_), "Target: %i C", static_cast<int>(autotune_target(heater_)));
        snprintf(cycle_text_, sizeof(cycle_text_), "Cycles: 0/%i", static_cast<int>(pid_autotune_cycles));
        progress_.set_progress_percent(0);
    } else if (status.active) {
        snprintf(status_text_, sizeof(status_text_), "%s", status.heating ? "Heating" : "Cooling");
        snprintf(
            temperature_text_,
            sizeof(temperature_text_),
            "Temp: %.1f / %.0f C",
            static_cast<double>(status.current),
            static_cast<double>(status.target));
        snprintf(cycle_text_, sizeof(cycle_text_), "Cycles: %i/%i", static_cast<int>(status.cycle), static_cast<int>(status.total_cycles));
        progress_.set_progress_percent(status.progress);
    } else if (status.finished) {
        finished_ = true;
        snprintf(status_text_, sizeof(status_text_), "%s", status.failed ? "Autotune failed" : "Autotune complete.");
        snprintf(temperature_text_, sizeof(temperature_text_), "Temp: %.1f C", static_cast<double>(status.current));
        snprintf(cycle_text_, sizeof(cycle_text_), "%s", status.failed ? "Check heater and retry." : "New PID values are ready.");
        progress_.set_progress_percent(status.progress);
        radio_.Change(PhaseResponses { Response::Done, Response::_none, Response::_none, Response::_none });
        prompt_apply(status);
    }

    status_.SetText(string_view_utf8::MakeRAM(status_text_));
    temperature_.SetText(string_view_utf8::MakeRAM(temperature_text_));
    cycle_.SetText(string_view_utf8::MakeRAM(cycle_text_));
}

void ScreenPidAutotune::prompt_apply(const pid_autotune_status::Snapshot &status) {
    if (prompted_ || status.failed) {
        return;
    }

    prompted_ = true;
    char prompt[160] = {};
    snprintf(
        prompt,
        sizeof(prompt),
        "New PID values:\nP %.2f\nI %.2f\nD %.2f\n\nSave these values?",
        static_cast<double>(status.p),
        static_cast<double>(status.i),
        static_cast<double>(status.d));
    if (MsgBoxQuestion(string_view_utf8::MakeRAM(prompt), Responses_YesNo) != Response::Yes) {
        snprintf(status_text_, sizeof(status_text_), "%s", "New PID values discarded.");
        snprintf(temperature_text_, sizeof(temperature_text_), "%s", "Existing values kept.");
        snprintf(cycle_text_, sizeof(cycle_text_), "P %.2f  I %.2f  D %.2f", static_cast<double>(status.p), static_cast<double>(status.i), static_cast<double>(status.d));
        status_.SetText(string_view_utf8::MakeRAM(status_text_));
        temperature_.SetText(string_view_utf8::MakeRAM(temperature_text_));
        cycle_.SetText(string_view_utf8::MakeRAM(cycle_text_));
        return;
    }

    switch (heater_) {
    case PidHeater::hotend:
        apply_hotend_pid(status.p, status.i, status.d);
        break;
    case PidHeater::bed:
#if ENABLED(PIDTEMPBED)
        apply_bed_pid(status.p, status.i, status.d);
#endif
        break;
    }
    const bool save_submitted = marlin_client::gcode_try("M500") == marlin_client::GcodeTryResult::Submitted;
    snprintf(status_text_, sizeof(status_text_), "%s", save_submitted ? "PID values saved." : "PID save failed.");
    snprintf(temperature_text_, sizeof(temperature_text_), "%s", save_submitted ? "Autotune succeeded." : "Try saving again.");
    snprintf(cycle_text_, sizeof(cycle_text_), "P %.2f  I %.2f  D %.2f", static_cast<double>(status.p), static_cast<double>(status.i), static_cast<double>(status.d));
    status_.SetText(string_view_utf8::MakeRAM(status_text_));
    temperature_.SetText(string_view_utf8::MakeRAM(temperature_text_));
    cycle_.SetText(string_view_utf8::MakeRAM(cycle_text_));
}

void ScreenPidAutotune::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::LOOP:
        update_status();
        break;
    case GUI_event_t::CHILD_CLICK:
        switch (event_conversion_union { .pvoid = param }.response) {
        case Response::Abort:
            marlin_client::gcode("M108");
            Screens::Access()->Close();
            return;
        case Response::Done:
            if (finished_) {
                Screens::Access()->Close();
                return;
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    screen_t::windowEvent(sender, event, param);
}

ScreenPidAutotuneHotend::ScreenPidAutotuneHotend()
    : ScreenPidAutotune(PidHeater::hotend) {}

ScreenPidAutotuneBed::ScreenPidAutotuneBed()
    : ScreenPidAutotune(PidHeater::bed) {}

ScreenMenuPidHotend::ScreenMenuPidHotend()
    : screen_menu_pid::HotendScreenBase(_("HOTEND PID")) {}

void ScreenMenuPidHotend::reload_items() {
#if ENABLED(PIDTEMP)
    Item<MI_PID_HOTEND_P>().reload();
    Item<MI_PID_HOTEND_I>().reload();
    Item<MI_PID_HOTEND_D>().reload();
#endif
}

void ScreenMenuPidHotend::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::LOOP && pid_autotune_status::snapshot().finished) {
        reload_items();
        pid_autotune_status::clear();
    }

    if (event == GUI_event_t::CHILD_CLICK) {
#if ENABLED(PIDTEMP)
        if (param == &Item<MI_PID_RESET_HOTEND>()) {
            reset_hotend_pid();
            reload_items();
            return;
        }
#endif
    }

    screen_menu_pid::HotendScreenBase::windowEvent(sender, event, param);
}

ScreenMenuPidBed::ScreenMenuPidBed()
    : screen_menu_pid::BedScreenBase(_("HEATBED PID")) {}

void ScreenMenuPidBed::reload_items() {
#if ENABLED(PIDTEMPBED)
    Item<MI_PID_BED_P>().reload();
    Item<MI_PID_BED_I>().reload();
    Item<MI_PID_BED_D>().reload();
#endif
}

void ScreenMenuPidBed::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::LOOP && pid_autotune_status::snapshot().finished) {
        reload_items();
        pid_autotune_status::clear();
    }

    if (event == GUI_event_t::CHILD_CLICK) {
#if ENABLED(PIDTEMPBED)
        if (param == &Item<MI_PID_RESET_BED>()) {
            reset_bed_pid();
            reload_items();
            return;
        }
#endif
    }

    screen_menu_pid::BedScreenBase::windowEvent(sender, event, param);
}

ScreenMenuPid::ScreenMenuPid()
    : screen_menu_pid::ScreenBase(_("PID SETTINGS")) {
}

#pragma once

#include <WindowMenuSpin.hpp>
#include <i_window_menu_item.hpp>
#include <pid_autotune_status.hpp>
#include <radio_button.hpp>
#include <screen_menu.hpp>
#include <window_header.hpp>
#include <window_progress.hpp>
#include <window_text.hpp>

enum class PidHeater : uint8_t {
    hotend,
    bed,
};

enum class PidTerm : uint8_t {
    p,
    i,
    d,
};

class MI_PID_VALUE : public WiSpin {
public:
    MI_PID_VALUE(PidHeater heater, PidTerm term, const char *label);
    void OnClick() override;

    void reload();

private:
    PidHeater heater_;
    PidTerm term_;
};

class MI_PID_HOTEND_P : public MI_PID_VALUE {
public:
    MI_PID_HOTEND_P();
};

class MI_PID_HOTEND_I : public MI_PID_VALUE {
public:
    MI_PID_HOTEND_I();
};

class MI_PID_HOTEND_D : public MI_PID_VALUE {
public:
    MI_PID_HOTEND_D();
};

class MI_PID_BED_P : public MI_PID_VALUE {
public:
    MI_PID_BED_P();
};

class MI_PID_BED_I : public MI_PID_VALUE {
public:
    MI_PID_BED_I();
};

class MI_PID_BED_D : public MI_PID_VALUE {
public:
    MI_PID_BED_D();
};

class MI_PID_RESET_HOTEND : public IWindowMenuItem {
public:
    MI_PID_RESET_HOTEND();
    void click(IWindowMenu &window_menu) override;
};

class MI_PID_AUTOTUNE_HOTEND : public IWindowMenuItem {
public:
    MI_PID_AUTOTUNE_HOTEND();
    void click(IWindowMenu &window_menu) override;
};

class MI_PID_RESET_BED : public IWindowMenuItem {
public:
    MI_PID_RESET_BED();
    void click(IWindowMenu &window_menu) override;
};

class MI_PID_AUTOTUNE_BED : public IWindowMenuItem {
public:
    MI_PID_AUTOTUNE_BED();
    void click(IWindowMenu &window_menu) override;
};

namespace screen_menu_pid {
using ScreenBase = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN
#if ENABLED(PIDTEMP)
    , MI_PID_HOTEND_P, MI_PID_HOTEND_I, MI_PID_HOTEND_D, MI_PID_AUTOTUNE_HOTEND, MI_PID_RESET_HOTEND
#endif
#if ENABLED(PIDTEMPBED)
    , MI_PID_BED_P, MI_PID_BED_I, MI_PID_BED_D, MI_PID_AUTOTUNE_BED, MI_PID_RESET_BED
#endif
    >;
} // namespace screen_menu_pid

class ScreenPidAutotune : public screen_t {
public:
    explicit ScreenPidAutotune(PidHeater heater);

private:
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
    void update_status();
    void start_autotune();
    void prompt_apply(const pid_autotune_status::Snapshot &status);

    PidHeater heater_;
    bool command_sent_ = false;
    bool finished_ = false;
    bool prompted_ = false;
    char status_text_[80] = {};
    char temperature_text_[48] = {};
    char cycle_text_[32] = {};
    window_header_t header_;
    window_text_t status_;
    window_text_t temperature_;
    window_text_t cycle_;
    WindowProgressBar progress_;
    RadioButton radio_;
};

class ScreenPidAutotuneHotend : public ScreenPidAutotune {
public:
    ScreenPidAutotuneHotend();
};

class ScreenPidAutotuneBed : public ScreenPidAutotune {
public:
    ScreenPidAutotuneBed();
};

class ScreenMenuPid : public screen_menu_pid::ScreenBase {
public:
    ScreenMenuPid();

private:
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
    void reload_items();
};

#include "screen_printing_serial.hpp"
#include "config.h"
#include "marlin_client.hpp"
#include "filament.hpp"
#include "i18n.h"
#include "ScreenHandler.hpp"
#include "odometer.hpp"
#include "config_features.h"
#include <config_store/store_instance.hpp>
#include <display.hpp>
#include <printer_lock.hpp>
#include "screen_menu_tune.hpp"
#include "img_resources.hpp"
#include "screen_printing_end_result.hpp"
#include <serial_printing.hpp>
#include <fsm_loadunload_type.hpp>
#include <marlin_server_types/client_response.hpp>
#include <fsm/safety_timer_phases.hpp>
#include <ui_theme.hpp>
#include <feature/print_status_message/print_status_message_formatter_buddy.hpp>
#include <feature/print_status_message/print_status_message_mgr.hpp>
#include <option/has_coldpull.h>
#include <option/has_loadcell.h>
#if HAS_LOADCELL()
    #include <fsm/nozzle_cleaning_failed_phases.hpp>
#endif
#if HAS_COLDPULL()
    #include <common/cold_pull.hpp>
#endif
#include <utils/string_builder.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <option/has_leds.h>
#include <option/has_auto_retract.h>
#include <option/has_chamber_api.h>
#include <option/has_chamber_filtration_api.h>
#if HAS_CHAMBER_FILTRATION_API()
    #include <feature/chamber_filtration/chamber_filtration.hpp>
#endif
#if HAS_LEDS()
    #include <leds/led_manager.hpp>
#endif
#if ENABLED(CRASH_RECOVERY)
    #include "../Marlin/src/feature/prusa/crash_recovery.hpp"
#endif

namespace {
point_i16_t get_terminal_location() {
    constexpr int16_t term_width = width(GuiDefaults::DefaultFont) * screen_printing_serial_data_t::terminal_columns;
    constexpr auto x = GuiDefaults::RectScreenBody.Left() + (GuiDefaults::RectScreenBody.Width() - term_width) / 2;
    return point_i16_t { x, GuiDefaults::RectScreenBody.Top() };
}

Rect16 get_legacy_logo_rect() {
    return Rect16(
        (GuiDefaults::RectScreen.Width() - img::serial_printing_172x138.w) / 2,
        GuiDefaults::RectScreenBody.Top() + (GuiDefaults::RectScreenBody.Height() - img::serial_printing_172x138.h) / 2,
        img::serial_printing_172x138.w,
        img::serial_printing_172x138.h);
}

#if HAS_MINI_DISPLAY()
constexpr auto etime_val_font { Font::small };
constexpr Rect16 progress_rect { 10, 70, GuiDefaults::RectScreen.Width() - 2 * 10, 16 };
constexpr Rect16 progress_txt_rect { 10, 86, GuiDefaults::RectScreen.Width() - 2 * 10, 30 };
constexpr Rect16 etime_label_rect { 10, 128, GuiDefaults::RectScreen.Width() - 2 * 10, 20 };
constexpr Rect16 etime_value_rect { 10, 148, GuiDefaults::RectScreen.Width() - 2 * 10, 20 };
constexpr Rect16 time_label_rect { 0, 0, 0, 0 };
constexpr Rect16 time_value_rect { 0, 0, 0, 0 };
constexpr Rect16 status_label_rect { 10, 66, GuiDefaults::RectScreen.Width() - 2 * 10, 20 };
constexpr Rect16 status_value_rect { 10, 92, GuiDefaults::RectScreen.Width() - 2 * 10, 52 };
constexpr Rect16 status_progress_rect { 10, 160, GuiDefaults::RectScreen.Width() - 2 * 10, 16 };
constexpr Rect16 message_label_rect { 10, 94, GuiDefaults::RectScreen.Width() - 2 * 10, 20 };
constexpr Rect16 message_value_rect { 10, 116, GuiDefaults::RectScreen.Width() - 2 * 10, 54 };
constexpr Rect16 time_dots_rect { 10, 171, 44, 6 };
constexpr Rect16 page_dots_rect { 10, 174, 44, 6 };
#elif HAS_LARGE_DISPLAY()
constexpr auto etime_val_font { Font::normal };
constexpr Rect16 progress_rect { 30, 65, GuiDefaults::RectScreen.Width() - 2 * 30, 16 };
constexpr size_t row_0 { 104 };
constexpr size_t row_height { 20 };
constexpr size_t get_row(size_t idx) {
    return row_0 + idx * row_height;
}
const Rect16 progress_txt_rect { EndResultBody::get_progress_txt_rect(row_0) };
constexpr Rect16 etime_label_rect { 30, get_row(0), 150, 20 };
constexpr Rect16 etime_value_rect { 30, get_row(1), 200, 23 };
constexpr Rect16 time_label_rect { 0, 0, 0, 0 };
constexpr Rect16 time_value_rect { 0, 0, 0, 0 };
constexpr Rect16 status_label_rect { 30, 74, 420, 24 };
constexpr Rect16 status_value_rect { 30, 104, 420, 48 };
constexpr Rect16 status_progress_rect { 30, 160, GuiDefaults::RectScreen.Width() - 2 * 30, 16 };
constexpr Rect16 message_label_rect { 30, 90, 420, 24 };
constexpr Rect16 message_value_rect { 30, 116, 420, 56 };
constexpr Rect16 time_dots_rect { 30, get_row(1) + height(Font::normal) + 5, 44, 6 };
constexpr Rect16 page_dots_rect { 30, 174, 44, 6 };
#endif
constexpr int32_t page_rotation_s = 4;

bool ascii_iequals(char a, char b) {
    if (a >= 'A' && a <= 'Z') {
        a = a - 'A' + 'a';
    }
    if (b >= 'A' && b <= 'Z') {
        b = b - 'A' + 'a';
    }
    return a == b;
}

bool contains_case_insensitive(const char *haystack, const char *needle) {
    if (*needle == '\0') {
        return true;
    }

    for (const char *h = haystack; *h; ++h) {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np && ascii_iequals(*hp, *np)) {
            ++hp;
            ++np;
        }
        if (*np == '\0') {
            return true;
        }
    }

    return false;
}

const char *skip_spaces(const char *text) {
    while (*text == ' ') {
        ++text;
    }
    return text;
}

bool consume_word_case_insensitive(const char *&text, const char *word) {
    const char *tp = text;
    const char *wp = word;
    while (*tp && *wp && ascii_iequals(*tp, *wp)) {
        ++tp;
        ++wp;
    }
    if (*wp != '\0') {
        return false;
    }
    text = tp;
    return true;
}

bool is_progress_or_eta_message(const char *message) {
    return contains_case_insensitive(message, "eta")
        || contains_case_insensitive(message, "ets")
        || contains_case_insensitive(message, "etl")
        || contains_case_insensitive(message, "estimated")
        || contains_case_insensitive(message, "finish")
        || contains_case_insensitive(message, "complete at")
        || contains_case_insensitive(message, "done at")
        || contains_case_insensitive(message, "will end")
        || contains_case_insensitive(message, "remaining")
        || contains_case_insensitive(message, "time left")
        || contains_case_insensitive(message, "left:")
        || contains_case_insensitive(message, "progress")
        || contains_case_insensitive(message, "%");
}

bool is_unimportant_host_message(const char *message) {
    return contains_case_insensitive(message, "good")
        && contains_case_insensitive(message, "accuracy");
}

bool is_clock_message(const char *message) {
    char *end = nullptr;
    const auto hour = strtol(message, &end, 10);
    if (end == message || hour < 0 || hour > 23 || *end != ':') {
        return false;
    }

    const char *minute_start = end + 1;
    const auto minute = strtol(minute_start, &end, 10);
    if (end == minute_start || minute < 0 || minute > 59) {
        return false;
    }

    if (*end == ':') {
        const char *second_start = end + 1;
        const auto second = strtol(second_start, &end, 10);
        if (end == second_start || second < 0 || second > 59) {
            return false;
        }
    }

    const char *suffix = skip_spaces(end);

    if ((suffix[0] == 'A' || suffix[0] == 'a' || suffix[0] == 'P' || suffix[0] == 'p') && (suffix[1] == 'M' || suffix[1] == 'm')) {
        suffix += 2;
        suffix = skip_spaces(suffix);
    }

    if (consume_word_case_insensitive(suffix, "today") || consume_word_case_insensitive(suffix, "tomorrow")) {
        suffix = skip_spaces(suffix);
    }

    return *suffix == '\0';
}

bool should_show_host_message(const char *message) {
    return !is_progress_or_eta_message(message)
        && !is_unimportant_host_message(message)
        && !is_clock_message(message);
}

template <size_t N>
void append_message_line(std::array<char, N> &target, const char *message) {
    if (*message == '\0') {
        return;
    }

    if (target[0] == '\0') {
        strlcpy(target.data(), message, target.size());
        return;
    }

    while (strlen(target.data()) + 1 + strlen(message) + 1 > target.size()) {
        char *next_line = strchr(target.data(), '\n');
        if (next_line == nullptr) {
            target[0] = '\0';
            break;
        }
        memmove(target.data(), next_line + 1, strlen(next_line + 1) + 1);
    }

    if (target[0] == '\0') {
        strlcpy(target.data(), message, target.size());
    } else {
        strlcat(target.data(), "\n", target.size());
        strlcat(target.data(), message, target.size());
    }
}

float percent_from_progress(const PrintStatusMessageDataProgress &progress) {
    if (progress.target <= 0) {
        return 0;
    }
    return std::clamp((progress.current * 100.0f) / progress.target, 0.0f, 100.0f);
}

float percent_from_raw_value(float value) {
    return std::clamp(value, 0.0f, 100.0f);
}

PrintStatusMessageManager::Record current_non_custom_status_message(uint32_t minimum_id) {
    auto current = print_status_message().current_message();
    if (current && current.id > minimum_id && current.message.type != PrintStatusMessage::custom) {
        return current;
    }

    PrintStatusMessageManager::Record latest;
    print_status_message().walk_history([&latest, minimum_id](const PrintStatusMessageManager::Record &msg) {
        if (msg.id > minimum_id && msg.message.type != PrintStatusMessage::custom) {
            latest = msg;
        }
        return true;
    });
    return latest;
}

void copy_first_line(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }

    const char *end = src;
    while (*end != '\0' && *end != '\n') {
        ++end;
    }

    const size_t copy_len = std::min<size_t>(dst_size - 1, end - src);
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

const char *load_unload_status_label(PhasesLoadUnload phase, LoadUnloadMode mode) {
    switch (phase) {
    case PhasesLoadUnload::Purging_stoppable:
    case PhasesLoadUnload::Purging_unstoppable:
    case PhasesLoadUnload::IsColorPurge:
        return N_("Purging");
    case PhasesLoadUnload::Unloading_stoppable:
    case PhasesLoadUnload::Unloading_unstoppable:
    case PhasesLoadUnload::Ejecting_stoppable:
    case PhasesLoadUnload::Ejecting_unstoppable:
#if HAS_NOZZLE_CLEANER()
    case PhasesLoadUnload::UnloadNozzleCleaning:
#endif
#if HAS_MMU2()
    case PhasesLoadUnload::MMU_UnloadingFilament:
    case PhasesLoadUnload::MMU_UnloadingToFinda:
    case PhasesLoadUnload::MMU_UnloadingToPulley:
    case PhasesLoadUnload::MMU_EjectingFilament:
    case PhasesLoadUnload::MMU_RetractingFromFinda:
#endif
        return N_("Unloading");
    case PhasesLoadUnload::ChangingTool:
        return N_("Changing filament");
    default:
        break;
    }

    switch (mode) {
    case LoadUnloadMode::Change:
        return N_("Changing filament");
    case LoadUnloadMode::Unload:
    case LoadUnloadMode::Eject:
        return N_("Unloading");
    case LoadUnloadMode::Purge:
        return N_("Purging");
    default:
        return N_("Loading");
    }
}

bool load_unload_phase_has_progress_data(PhasesLoadUnload phase) {
    switch (phase) {
    case PhasesLoadUnload::initial:
#if HAS_MMU2()
    case PhasesLoadUnload::MMUDummyStartNoAttention:
    case PhasesLoadUnload::MMU_ERRWaitingForUser:
#endif
    case PhasesLoadUnload::_cnt:
        return false;
    default:
        return true;
    }
}

#if HAS_LOADCELL()
bool nozzle_cleaning_phase_has_progress_data(PhaseNozzleCleaningFailed phase) {
    switch (phase) {
#if HAS_NOZZLE_CLEANING_FAILED_PURGING()
    case PhaseNozzleCleaningFailed::wait_temp:
    case PhaseNozzleCleaningFailed::purge:
    case PhaseNozzleCleaningFailed::autoretract:
        return true;
#endif
    default:
        return false;
    }
}
#endif

#if HAS_COLDPULL()
const char *cold_pull_status_label(PhasesColdPull phase) {
    switch (phase) {
#if HAS_AUTO_RETRACT()
    case PhasesColdPull::deretract:
        return N_("Heating up");
#endif
    case PhasesColdPull::cool_down:
        return N_("Cooling down");
    case PhasesColdPull::heat_up:
        return N_("Heating up");
    default:
        return nullptr;
    }
}
#endif

bool post_print_filtration_active() {
#if HAS_CHAMBER_FILTRATION_API()
    return buddy::chamber_filtration().post_print_remaining_s() > 0;
#else
    return false;
#endif
}

void stop_post_print_filtration() {
#if HAS_CHAMBER_FILTRATION_API()
    buddy::chamber_filtration().stop_post_print_filtration();
#endif
}
} // namespace

screen_printing_serial_data_t::screen_printing_serial_data_t()
    : ScreenPrintingModel(_(caption))
    , last_tick(0)
    , connection(connection_state_t::connected)
    , term_buff()
    , term(this, get_terminal_location(), &term_buff)
    , w_progress(this, progress_rect)
    , w_progress_txt(this, progress_txt_rect)
    , w_etime_label(this, etime_label_rect, is_multiline::no)
    , w_etime_value(this, etime_value_rect, is_multiline::no)
    , w_time_label(this, time_label_rect, is_multiline::no)
    , w_time_value(this, time_value_rect, is_multiline::no)
    , w_status_label(this, status_label_rect, is_multiline::no)
    , w_status_value(this, status_value_rect, is_multiline::yes)
    , w_status_progress(this, status_progress_rect, ui_theme::progress(), COLOR_GRAY)
    , w_message_label(this, message_label_rect, is_multiline::no)
    , w_message_value(this, message_value_rect, is_multiline::yes)
    , time_dots(this, time_dots_rect, static_cast<uint8_t>(TimeItem::_count))
    , page_dots(this, page_dots_rect, 2)
    , last_state(marlin_server::State::Aborted) {
    ClrMenuTimeoutClose();
    ClrOnSerialClose();

    SetButtonIconAndLabel(BtnSocket::Right, BtnRes::Disconnect, LabelRes::Stop);

    term.Hide();

    w_progress_txt.set_font(EndResultBody::progress_font);
#if HAS_MINI_DISPLAY()
    w_progress_txt.SetAlignment(Align_t::Center());
#else
    w_progress_txt.SetAlignment(EndResultBody::progress_alignment);
#endif

    w_etime_label.set_font(Font::small);
    w_etime_label.SetTextColor(COLOR_SILVER);
    w_etime_label.SetAlignment(Align_t::LeftBottom());
    w_etime_label.SetText(_(PrintTime::EN_STR_COUNTDOWN));

    w_etime_value.set_font(etime_val_font);
    w_etime_value.SetAlignment(Align_t::LeftBottom());
    w_etime_value.SetPadding({ 0, 2, 0, 2 });

    w_time_label.set_font(Font::small);
    w_time_label.SetTextColor(COLOR_SILVER);
    w_time_label.SetAlignment(Align_t::LeftBottom());
    w_time_label.SetText(_(PrintTime::EN_STR_TIMESTAMP));

    w_time_value.set_font(Font::normal);
    w_time_value.SetAlignment(Align_t::LeftTop());
    w_time_value.SetPadding({ 0, 0, 0, 2 });

    w_status_label.set_font(HAS_MINI_DISPLAY() ? Font::normal : Font::big);
    w_status_label.SetTextColor(COLOR_WHITE);
    w_status_label.SetText(_("Preparing"));

    w_status_value.set_font(HAS_MINI_DISPLAY() ? Font::normal : Font::big);
    w_status_value.SetTextColor(COLOR_WHITE);
    w_status_value.SetAlignment(Align_t::Center());
    w_status_value.SetPadding({ 0, 2, 0, 2 });

    w_message_label.set_font(Font::small);
    w_message_label.SetTextColor(COLOR_SILVER);
    w_message_label.SetText(_("Message"));

    w_message_value.set_font(Font::small);
    w_message_value.SetTextColor(COLOR_WHITE);
    w_message_value.SetAlignment(Align_t::LeftTop());
    w_message_value.SetPadding({ 0, 2, 0, 2 });

    time_dots.set_one_circle_mode(true);
    time_dots.Hide();

    page_dots.set_one_circle_mode(true);
    page_dots.Hide();

    last_message_id = status_message_baseline_id = SerialPrinting::status_message_baseline();
    set_page(SerialPrinting::ui_mode() == SerialPrintingUiMode::legacy ? Page::legacy : Page::initializing);
    update_progress();
}

void screen_printing_serial_data_t::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    marlin_server::State state = marlin_vars().print_state;

    if (state != last_state) {
        if (state == marlin_server::State::Finished) {
            header.SetText(_("PRINT FINISHED"));
            finished_stat = FinishedStat::duration;
            last_finished_stat_switch_s = ticks_s();
            update_finished_summary();
            set_page(Page::message);
        } else if (last_state == marlin_server::State::Finished) {
            header.SetText(_(caption));
        }

        if (printer_lock::locked()) {
            show_locked_buttons();
            lock_buttons_applied = true;
            last_state = state;
        } else {
            update_action_buttons(state);
            last_state = state;
        }
    }

    switch (event) {
    case GUI_event_t::LOOP:
        if (printer_lock::locked()) {
            if (!lock_buttons_applied) {
                show_locked_buttons();
                lock_buttons_applied = true;
            }
        } else if (lock_buttons_applied) {
            lock_buttons_applied = false;
            update_action_buttons(state);
        }
        if (!printer_lock::locked() && state == marlin_server::State::Finished) {
            update_action_buttons(state);
        }

        update_progress();
        update_status();
        update_messages();
        if (state == marlin_server::State::Finished) {
            update_finished_summary();
        } else {
            if (SerialPrinting::ui_mode() == SerialPrintingUiMode::legacy) {
                set_page(Page::legacy);
            } else if (!serial_data_seen) {
                set_page(Page::initializing);
            } else if (status_page_available()) {
                user_selected_page = false;
                set_page(Page::status);
            } else if (current_page == Page::status && !status_page_available()) {
                set_page(Page::progress);
            } else if (!user_selected_page && can_toggle_pages()) {
                const uint32_t now_s = ticks_s();
                if (ticks_diff(now_s, last_page_switch_s) >= page_rotation_s) {
                    advance_page();
                }
            } else if (!message_page_available() && current_page == Page::message) {
                set_page(Page::progress);
            }
        }
        update_page_dots();
        break;

    case GUI_event_t::HELD_RELEASED:
    case GUI_event_t::TOUCH_SWIPE_LEFT:
        toggle_page();
        break;

    case GUI_event_t::TOUCH_SWIPE_RIGHT:
        if (!can_toggle_pages()) {
            break;
        }
        user_selected_page = true;
        retreat_page();
        break;

    case GUI_event_t::TOUCH_SWIPE_UP:
        toggle_page();
        break;

    case GUI_event_t::TOUCH_SWIPE_DOWN:
        if (!can_toggle_pages()) {
            break;
        }
        user_selected_page = true;
        retreat_page();
        break;

    default:
        break;
    }

    ScreenPrintingModel::windowEvent(sender, event, param);
}

void screen_printing_serial_data_t::update_finished_summary() {
    const uint32_t now_s = ticks_s();
    if (ticks_diff(now_s, last_finished_stat_switch_s) >= page_rotation_s) {
        advance_finished_stat(true);
    }

    switch (finished_stat) {
    case FinishedStat::duration:
        w_message_label.SetText(_(EndResultBody::txt_printing_time));
        PrintTime::print_formatted_duration(marlin_vars().print_duration.get(), message_text, true);
        break;
    case FinishedStat::filtering:
#if HAS_CHAMBER_FILTRATION_API()
        w_message_label.SetText(_("Filtering left"));
        PrintTime::print_formatted_duration(buddy::chamber_filtration().post_print_remaining_s(), message_text, true);
#endif
        break;
    case FinishedStat::finished_at:
        w_message_label.SetText(_("Print ended"));
        EndResultBody::format_timestamp(marlin_vars().print_end_time, message_text);
        break;
    case FinishedStat::_count:
        break;
    }
    w_message_value.SetText(string_view_utf8::MakeRAM(message_text.data()));
    w_message_label.Invalidate();
    w_message_value.Invalidate();
    update_page_dots();
}

void screen_printing_serial_data_t::advance_finished_stat(bool forward) {
    do {
        const size_t index = static_cast<size_t>(finished_stat);
        const size_t count = static_cast<size_t>(FinishedStat::_count);
        finished_stat = static_cast<FinishedStat>((index + (forward ? 1 : count - 1)) % count);
    } while (!finished_stat_available(finished_stat));
    last_finished_stat_switch_s = ticks_s();
}

bool screen_printing_serial_data_t::finished_stat_available(FinishedStat stat) const {
    switch (stat) {
    case FinishedStat::duration:
    case FinishedStat::finished_at:
        return true;
    case FinishedStat::filtering:
#if HAS_CHAMBER_FILTRATION_API()
        return buddy::chamber_filtration().post_print_remaining_s() > 0;
#else
        return false;
#endif
    case FinishedStat::_count:
        return false;
    }
    return false;
}

size_t screen_printing_serial_data_t::finished_stat_count() const {
    size_t count = 0;
    for (size_t i = 0; i < static_cast<size_t>(FinishedStat::_count); ++i) {
        count += finished_stat_available(static_cast<FinishedStat>(i));
    }
    return count;
}

size_t screen_printing_serial_data_t::finished_stat_index() const {
    size_t index = 0;
    for (size_t i = 0; i < static_cast<size_t>(FinishedStat::_count); ++i) {
        const auto stat = static_cast<FinishedStat>(i);
        if (!finished_stat_available(stat)) {
            continue;
        }
        if (stat == finished_stat) {
            return index;
        }
        ++index;
    }
    return 0;
}

void screen_printing_serial_data_t::unconditionalDraw() {
    ScreenPrintingModel::unconditionalDraw();

    if (current_page != Page::legacy) {
        return;
    }

    ropfn raster_op;
    raster_op.shadow = is_shadowed::no;
    raster_op.swap_bw = has_swapped_bw::no;
    const auto rect = get_legacy_logo_rect();
    display::draw_img(point_ui16(rect.Left(), rect.Top()), img::serial_printing_172x138, GetBackColor(), raster_op);
}

void screen_printing_serial_data_t::update_action_buttons(marlin_server::State state) {
    SetButtonIconAndLabel(BtnSocket::Left, BtnRes::Settings, LabelRes::Settings);
    SetButtonIconAndLabel(BtnSocket::Right, BtnRes::Disconnect, LabelRes::Stop);
    EnableButton(BtnSocket::Left);
    EnableButton(BtnSocket::Right);

    switch (state) {
    case marlin_server::State::Finished:
        if (post_print_filtration_active()) {
            EnableButton(BtnSocket::Middle);
            SetButtonIconAndLabel(BtnSocket::Middle, BtnRes::Stop, LabelRes::StopFilter);
        } else {
            DisableButton(BtnSocket::Middle);
        }
        SetButtonIconAndLabel(BtnSocket::Right, BtnRes::SetReady, LabelRes::Continue);
        break;
    case marlin_server::State::Aborted:
        DisableButton(BtnSocket::Middle);
        SetButtonIconAndLabel(BtnSocket::Right, BtnRes::SetReady, LabelRes::Continue);
        break;
    case marlin_server::State::Paused:
        SetButtonIconAndLabel(BtnSocket::Middle, BtnRes::Resume, LabelRes::Resume);
        EnableButton(BtnSocket::Middle);
        break;
    case marlin_server::State::Printing:
        SetButtonIconAndLabel(BtnSocket::Middle, BtnRes::Pause, LabelRes::Pause);
        EnableButton(BtnSocket::Middle);
        break;
    default:
        DisableButton(BtnSocket::Middle);
        break;
    }
}

void screen_printing_serial_data_t::update_progress() {
    const uint32_t time_to_end = marlin_vars().time_to_end.get();
    const uint32_t time_to_change = marlin_vars().time_to_pause.get();
    uint8_t host_percent = 0;
    uint32_t host_eta = 0;
    if (SerialPrinting::host_progress_percent(host_percent, ticks_ms())
        || SerialPrinting::host_time_to_end(host_eta, ticks_ms())) {
        serial_data_seen = true;
    }

    if (!time_item_available(current_time_item)) {
        current_time_item = first_time_item();
    }

    switch (current_time_item) {
    case TimeItem::end_time:
        w_etime_label.SetText(_(PrintTime::EN_STR_TIMESTAMP));
        if (!PrintTime::print_end_time(time_to_end, w_etime_value_buffer)) {
            strlcpy(w_etime_value_buffer.data(), "--", w_etime_value_buffer.size());
        }
        break;
    case TimeItem::remaining_time:
        w_etime_label.SetText(_(PrintTime::EN_STR_COUNTDOWN));
        PrintTime::print_formatted_duration(time_to_end, w_etime_value_buffer);
        break;
    case TimeItem::time_to_change:
        w_etime_label.SetText(_("Next change in"));
        PrintTime::print_formatted_duration(time_to_change, w_etime_value_buffer);
        break;
    case TimeItem::time_since_start:
        w_etime_label.SetText(_(EndResultBody::txt_printing_time));
        PrintTime::print_formatted_duration(marlin_vars().print_duration.get(), w_etime_value_buffer, true);
        break;
    case TimeItem::_count:
        current_time_item = TimeItem::time_since_start;
        break;
    }

    if (marlin_vars().print_speed != 100 && current_time_item != TimeItem::time_since_start) {
        strlcat(w_etime_value_buffer.data(), "?", w_etime_value_buffer.size());
    }

    w_etime_value.SetText(string_view_utf8::MakeRAM(w_etime_value_buffer.data()));
    w_etime_value.Invalidate();

    update_message_label();
}

void screen_printing_serial_data_t::update_message_label(bool force) {
    if (current_page != Page::message) {
        return;
    }

    const marlin_server::State state = marlin_vars().print_state;
    if (state == marlin_server::State::Finished || state == marlin_server::State::Aborted) {
        return;
    }

    const uint8_t percent = marlin_vars().sd_percent_done;
    if (!force && percent == last_message_progress_percent) {
        return;
    }

    last_message_progress_percent = percent;
    snprintf(w_etime_value_buffer.data(), w_etime_value_buffer.size(), "Messages %u%%", static_cast<unsigned>(percent));
    w_message_label.SetText(string_view_utf8::MakeRAM(w_etime_value_buffer.data()));
    w_message_label.Invalidate();
}

bool screen_printing_serial_data_t::time_item_available(TimeItem item) const {
    const uint32_t time_to_end = marlin_vars().time_to_end.get();
    const uint32_t time_to_change = marlin_vars().time_to_pause.get();
    const bool end_time_valid = time_to_end != marlin_server::TIME_TO_END_INVALID && time_to_end <= 60 * 60 * 24 * 365;

    switch (item) {
    case TimeItem::remaining_time:
    case TimeItem::end_time:
        return end_time_valid;
    case TimeItem::time_to_change:
        return time_to_change != marlin_server::TIME_TO_END_INVALID;
    case TimeItem::time_since_start:
        return true;
    case TimeItem::_count:
        return false;
    }
    return false;
}

screen_printing_serial_data_t::TimeItem screen_printing_serial_data_t::next_time_item(TimeItem item) const {
    auto next = item;
    do {
        next = static_cast<TimeItem>((static_cast<size_t>(next) + 1) % static_cast<size_t>(TimeItem::_count));
    } while (!time_item_available(next));
    return next;
}

screen_printing_serial_data_t::TimeItem screen_printing_serial_data_t::previous_time_item(TimeItem item) const {
    auto previous = item;
    do {
        previous = static_cast<TimeItem>(
            (static_cast<size_t>(previous) + static_cast<size_t>(TimeItem::_count) - 1) % static_cast<size_t>(TimeItem::_count));
    } while (!time_item_available(previous));
    return previous;
}

screen_printing_serial_data_t::TimeItem screen_printing_serial_data_t::first_time_item() const {
    for (size_t i = 0; i < static_cast<size_t>(TimeItem::_count); ++i) {
        const auto item = static_cast<TimeItem>(i);
        if (time_item_available(item)) {
            return item;
        }
    }
    return TimeItem::time_since_start;
}

screen_printing_serial_data_t::TimeItem screen_printing_serial_data_t::last_time_item() const {
    for (size_t i = static_cast<size_t>(TimeItem::_count); i > 0; --i) {
        const auto item = static_cast<TimeItem>(i - 1);
        if (time_item_available(item)) {
            return item;
        }
    }
    return TimeItem::time_since_start;
}

void screen_printing_serial_data_t::update_status() {
    const auto active_fsm_state = [](ClientFSM client_fsm) -> std::optional<fsm::BaseData> {
        return marlin_vars().peek_fsm_states([client_fsm](const fsm::States &states) -> std::optional<fsm::BaseData> {
            const auto &state = states[client_fsm];
            if (state.has_value()) {
                return *state;
            }
            return std::nullopt;
        });
    };

    const auto set_progress_status = [&](const char *label, float progress_percent) {
        strlcpy(status_text.data(), label, status_text.size());
        w_status_label.SetText(_(label));
        w_status_label.Invalidate();

        const int rounded_progress = static_cast<int>(std::round(std::clamp(progress_percent, 0.0f, 100.0f)));
        snprintf(status_value_text.data(), status_value_text.size(), "%i%%", rounded_progress);
        w_status_value.SetText(string_view_utf8::MakeRAM(status_value_text.data()));
        w_status_value.Invalidate();
        w_status_value.set_visible(current_page == Page::status);

        w_status_progress.set_progress_percent(rounded_progress);
        status_progress_available = true;
        w_status_progress.set_visible(current_page == Page::status);
    };

    const auto load_unload = active_fsm_state(ClientFSM::Load_unload);
    if (load_unload.has_value()) {
        const auto phase = GetEnumFromPhaseIndex<PhasesLoadUnload>(load_unload->GetPhase());
        if (load_unload_phase_has_progress_data(phase)) {
            const auto data = fsm::deserialize_data<FSMLoadUnloadData>(load_unload->GetData());
            set_progress_status(load_unload_status_label(phase, data.mode), data.progress);
            return;
        }
    }

#if HAS_LOADCELL()
    const auto nozzle_cleaning = active_fsm_state(ClientFSM::NozzleCleaningFailed);
    if (nozzle_cleaning.has_value()) {
        const auto phase = GetEnumFromPhaseIndex<PhaseNozzleCleaningFailed>(nozzle_cleaning->GetPhase());
        if (nozzle_cleaning_phase_has_progress_data(phase)) {
            const auto data = fsm::deserialize_data<NozzleCleaningFailedProgressData>(nozzle_cleaning->GetData());
            set_progress_status(N_("Nozzle cleaning"), static_cast<float>(data.progress_0_255) / 255.0f * 100.0f);
            return;
        }
    }
#endif

    const auto safety_timer = active_fsm_state(ClientFSM::SafetyTimer);
    if (safety_timer.has_value()) {
        const auto phase = GetEnumFromPhaseIndex<PhaseSafetyTimer>(safety_timer->GetPhase());
        if (phase == PhaseSafetyTimer::resuming) {
            set_progress_status(N_("Resuming temperatures"), fsm::deserialize_data<float>(safety_timer->GetData()));
            return;
        }
    }

#if HAS_COLDPULL()
    const auto cold_pull = active_fsm_state(ClientFSM::ColdPull);
    if (cold_pull.has_value()) {
        const auto phase = GetEnumFromPhaseIndex<PhasesColdPull>(cold_pull->GetPhase());
        if (const char *label = cold_pull_status_label(phase)) {
            const cold_pull::TemperatureProgressData data { cold_pull->GetData() };
            set_progress_status(label, data.percent);
            return;
        }
    }
#endif

    const auto current = current_non_custom_status_message(status_message_baseline_id);
    if (!current) {
        if (!serial_data_seen) {
            return;
        }
        status_text[0] = '\0';
        status_progress_available = false;
        return;
    }
    serial_data_seen = true;

    ArrayStringBuilder<256> label;
    PrintStatusMessageFormatterBuddy::format(label, current.message);
    copy_first_line(status_text.data(), status_text.size(), label.str());
    w_status_label.SetText(string_view_utf8::MakeRAM(status_text.data()));
    w_status_label.Invalidate();

    bool has_progress = false;
    float progress_percent = 0;
    status_value_text[0] = '\0';

    const auto set_progress = [&](const PrintStatusMessageDataProgress &progress, const char *format) {
        has_progress = true;
        progress_percent = percent_from_progress(progress);
        snprintf(status_value_text.data(), status_value_text.size(), format, (int)std::round(progress.current), (int)std::round(progress.target));
    };

    if (const auto data = std::get_if<PrintStatusMessageDataProgress>(&current.message.data)) {
        switch (current.message.type) {
        case PrintStatusMessage::probing_bed:
        case PrintStatusMessage::additional_probing:
            set_progress(*data, "%i/%i");
            break;
        case PrintStatusMessage::dwelling: {
            has_progress = true;
            progress_percent = percent_from_progress(*data);
            const int val = (int)std::round(data->current);
            snprintf(status_value_text.data(), status_value_text.size(), "%i:%02i", val / 60, val % 60);
            break;
        }
        case PrintStatusMessage::absorbing_heat:
#if HAS_AUTO_RETRACT()
        case PrintStatusMessage::auto_retracting:
#endif
            has_progress = true;
            progress_percent = percent_from_raw_value(data->current);
            snprintf(status_value_text.data(), status_value_text.size(), "%i%%", (int)std::round(progress_percent));
            break;
        case PrintStatusMessage::waiting_for_bed_temp:
#if HAS_CHAMBER_API()
        case PrintStatusMessage::waiting_for_chamber_temp:
#endif
            if (data->target <= 0) {
                snprintf(status_value_text.data(), status_value_text.size(), "%i C", (int)std::round(data->current));
            } else {
                set_progress(*data, "%i/%i C");
            }
            break;
        default:
            break;
        }
    } else if (const auto data = std::get_if<PrintStatusMessageDataToolProgress>(&current.message.data)) {
        has_progress = true;
        progress_percent = percent_from_progress(data->progress);
        snprintf(status_value_text.data(), status_value_text.size(), "%i/%i C", (int)std::round(data->progress.current), (int)std::round(data->progress.target));
    } else if (const auto data = std::get_if<PrintStatusMessageDataAxisProgress>(&current.message.data)) {
        has_progress = true;
        progress_percent = percent_from_progress(data->progress);
        snprintf(status_value_text.data(), status_value_text.size(), "%i%%", (int)std::round(progress_percent));
    }

    w_status_progress.set_progress_percent(progress_percent);
    status_progress_available = has_progress;
    w_status_progress.set_visible(current_page == Page::status && status_progress_available);

    if (status_value_text[0] != '\0') {
        w_status_value.SetText(string_view_utf8::MakeRAM(status_value_text.data()));
        w_status_value.Invalidate();
        w_status_value.set_visible(current_page == Page::status);
    } else {
        w_status_value.set_visible(false);
    }
}

void screen_printing_serial_data_t::update_messages() {
    print_status_message().walk_history([this](const PrintStatusMessageManager::Record &msg) {
        if (msg.id <= last_message_id) {
            return true;
        }

        serial_data_seen = true;

        ArrayStringBuilder<256> buf;
        PrintStatusMessageFormatterBuddy::format(buf, msg.message);
        if (should_show_host_message(buf.str())) {
            append_message_line(message_text, buf.str());
            w_message_value.SetText(string_view_utf8::MakeRAM(message_text.data()));
            w_message_value.Invalidate();
        }
        last_message_id = msg.id;
        return true;
    });
}

bool screen_printing_serial_data_t::message_page_available() const {
    return message_text[0] != '\0';
}

bool screen_printing_serial_data_t::status_page_available() const {
    return status_text[0] != '\0';
}

void screen_printing_serial_data_t::set_page(Page page) {
    if (SerialPrinting::ui_mode() == SerialPrintingUiMode::legacy && marlin_vars().print_state != marlin_server::State::Finished) {
        page = Page::legacy;
    }

    if (page == Page::status && !status_page_available()) {
        page = Page::progress;
    } else if (page == Page::message && !message_page_available()) {
        page = Page::progress;
    }

    current_page = page;
    last_page_switch_s = ticks_s();

    const bool show_message = current_page == Page::message;
    const bool show_progress = current_page == Page::progress;
    w_progress.set_visible(show_progress || show_message);
    w_progress_txt.set_visible(show_progress);
    w_etime_label.set_visible(show_progress);
    w_etime_value.set_visible(show_progress);
    w_time_label.set_visible(false);
    w_time_value.set_visible(false);
    time_dots.Hide();

    const bool show_status = current_page == Page::status || current_page == Page::initializing;
    w_status_label.set_visible(show_status);
    w_status_value.set_visible(show_status);
    w_status_progress.set_visible(show_status && status_progress_available);
    if (show_status) {
        time_dots.Hide();
    }

    w_message_label.set_visible(show_message);
    w_message_value.set_visible(show_message);
    if (show_message) {
        time_dots.Hide();
        update_message_label(true);
    }

    update_page_dots();
}

void screen_printing_serial_data_t::toggle_page() {
    if (!can_toggle_pages()) {
        return;
    }

    user_selected_page = true;
    if (marlin_vars().print_state == marlin_server::State::Finished) {
        advance_finished_stat(true);
        update_finished_summary();
        return;
    }
    advance_page();
}

bool screen_printing_serial_data_t::can_toggle_pages() const {
    if (marlin_vars().print_state == marlin_server::State::Finished) {
        return finished_stat_count() > 1;
    }
    return SerialPrinting::ui_mode() == SerialPrintingUiMode::progress && !status_page_available() && page_count() > 1;
}

void screen_printing_serial_data_t::advance_page() {
    if (current_page == Page::message) {
        current_time_item = first_time_item();
        set_page(Page::progress);
        return;
    }

    const auto next = next_time_item(current_time_item);
    if (message_page_available() && static_cast<size_t>(next) <= static_cast<size_t>(current_time_item)) {
        set_page(Page::message);
        return;
    }

    current_time_item = next;
    set_page(Page::progress);
}

void screen_printing_serial_data_t::retreat_page() {
    if (marlin_vars().print_state == marlin_server::State::Finished) {
        advance_finished_stat(false);
        update_finished_summary();
        return;
    }

    if (current_page == Page::message) {
        current_time_item = last_time_item();
        set_page(Page::progress);
        return;
    }

    if (message_page_available() && current_time_item == first_time_item()) {
        set_page(Page::message);
        return;
    }

    current_time_item = previous_time_item(current_time_item);
    set_page(Page::progress);
}

size_t screen_printing_serial_data_t::page_count() const {
    if (marlin_vars().print_state == marlin_server::State::Finished) {
        return finished_stat_count();
    }

    size_t count = 0;
    for (size_t i = 0; i < static_cast<size_t>(TimeItem::_count); ++i) {
        if (time_item_available(static_cast<TimeItem>(i))) {
            ++count;
        }
    }
    if (message_page_available()) {
        ++count;
    }
    return count;
}

size_t screen_printing_serial_data_t::current_page_index() const {
    if (marlin_vars().print_state == marlin_server::State::Finished) {
        return finished_stat_index();
    }

    size_t index = 0;
    for (size_t i = 0; i < static_cast<size_t>(TimeItem::_count); ++i) {
        const auto item = static_cast<TimeItem>(i);
        if (!time_item_available(item)) {
            continue;
        }
        if (current_page == Page::progress && item == current_time_item) {
            return index;
        }
        ++index;
    }
    return index;
}

void screen_printing_serial_data_t::update_page_dots() {
    const bool visible = can_toggle_pages();
    page_dots.set_visible(visible);
    if (visible) {
        page_dots.set_max_circles(static_cast<uint8_t>(page_count()));
        page_dots.set_index(static_cast<uint8_t>(current_page_index()));
    }
}

void screen_printing_serial_data_t::tuneAction() {
    if (printer_lock::locked()) {
        if (unlock_machine()) {
            last_state = marlin_server::State::Aborted;
        }
        return;
    }
    Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuTune>);
}

void screen_printing_serial_data_t::pauseAction() {
    if (printer_lock::locked()) {
        return;
    }
    // pause or resume button, depending on what state we are in
    marlin_server::State state = marlin_vars().print_state;
    switch (state) {
    case marlin_server::State::Finished:
        stop_post_print_filtration();
        update_action_buttons(state);
        break;
    case marlin_server::State::Paused:
        marlin_client::print_resume();
        break;
    case marlin_server::State::Printing:
        marlin_client::print_pause();
        break;
    default:
        break;
    }
}

void screen_printing_serial_data_t::stopAction() {
    if (printer_lock::locked()) {
        return;
    }
    const marlin_server::State state = marlin_vars().print_state;
    if (state == marlin_server::State::Finished || state == marlin_server::State::Aborted) {
#if HAS_LEDS()
        if (state == marlin_server::State::Finished) {
            leds::LEDManager::instance().acknowledge_finished();
        }
#endif
        marlin_client::print_exit();
        return;
    }
    if (MsgBoxWarning(_("Cancel this print?"), Responses_YesNo, 1)
        != Response::Yes) {
        return;
    }

    // abort print, disable button and wait for screen to close from marlin server
    marlin_client::print_abort();
    DisableButton(BtnSocket::Right);
}

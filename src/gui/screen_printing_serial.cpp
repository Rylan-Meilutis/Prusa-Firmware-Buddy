#include "screen_printing_serial.hpp"
#include "config.h"
#include "marlin_client.hpp"
#include "filament.hpp"
#include "i18n.h"
#include "ScreenHandler.hpp"
#include "odometer.hpp"
#include "config_features.h"
#include <config_store/store_instance.hpp>
#include <printer_lock.hpp>
#include "window_icon.hpp"
#include "screen_menu_tune.hpp"
#include "img_resources.hpp"
#include "screen_printing_end_result.hpp"
#include <serial_printing.hpp>
#include <ui_theme.hpp>
#include <feature/print_status_message/print_status_message_formatter_buddy.hpp>
#include <feature/print_status_message/print_status_message_mgr.hpp>
#include <utils/string_builder.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <option/has_leds.h>
#include <option/has_auto_retract.h>
#include <option/has_chamber_api.h>
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
        const auto second = strtol(end + 1, &end, 10);
        if (second < 0 || second > 59) {
            return false;
        }
    }

    while (*end == ' ') {
        ++end;
    }

    if (*end == '\0') {
        return true;
    }

    if ((end[0] == 'A' || end[0] == 'a' || end[0] == 'P' || end[0] == 'p') && (end[1] == 'M' || end[1] == 'm')) {
        end += 2;
        while (*end == ' ') {
            ++end;
        }
        return *end == '\0';
    }

    return false;
}

bool should_show_host_message(const char *message) {
    return !is_progress_or_eta_message(message)
        && !is_unimportant_host_message(message)
        && !is_clock_message(message);
}

void append_message_line(std::array<char, 256> &target, const char *message) {
    if (target[0] == '\0') {
        strlcpy(target.data(), message, target.size());
        return;
    }

    const size_t needed = strlen(target.data()) + 1 + strlen(message) + 1;
    if (needed > target.size()) {
        strlcpy(target.data(), message, target.size());
        return;
    }

    strlcat(target.data(), "\n", target.size());
    strlcat(target.data(), message, target.size());
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

PrintStatusMessageManager::Record current_non_custom_status_message() {
    auto current = print_status_message().current_message();
    if (current && current.message.type != PrintStatusMessage::custom) {
        return current;
    }

    PrintStatusMessageManager::Record latest;
    print_status_message().walk_history([&latest](const PrintStatusMessageManager::Record &msg) {
        if (msg.message.type != PrintStatusMessage::custom) {
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
    , legacy_logo(this, get_legacy_logo_rect(), &img::serial_printing_172x138)
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
    w_status_label.SetText(_("Preparing print"));

    w_status_value.set_font(HAS_MINI_DISPLAY() ? Font::normal : Font::big);
    w_status_value.SetTextColor(COLOR_WHITE);
    w_status_value.SetAlignment(Align_t::Center());
    w_status_value.SetPadding({ 0, 2, 0, 2 });

    w_message_label.set_font(Font::small);
    w_message_label.SetTextColor(COLOR_SILVER);
    w_message_label.SetText(_("Message"));

    w_message_value.set_font(Font::normal);
    w_message_value.SetTextColor(COLOR_WHITE);
    w_message_value.SetAlignment(Align_t::LeftTop());
    w_message_value.SetPadding({ 0, 2, 0, 2 });

    time_dots.set_one_circle_mode(true);
    time_dots.Hide();

    page_dots.set_one_circle_mode(true);
    page_dots.Hide();

    set_page(SerialPrinting::ui_mode() == SerialPrintingUiMode::legacy ? Page::legacy : Page::progress);
    update_progress();
}

void screen_printing_serial_data_t::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    marlin_server::State state = marlin_vars().print_state;

    if (state != last_state) {
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

        update_progress();
        update_status();
        update_messages();
        if (state == marlin_server::State::Finished) {
            strlcpy(message_text.data(), "Print finished", message_text.size());
            w_message_label.SetText(_("Status"));
            w_message_value.SetText(_("Print finished"));
            set_page(Page::message);
        } else if (SerialPrinting::ui_mode() == SerialPrintingUiMode::legacy) {
            set_page(Page::legacy);
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

void screen_printing_serial_data_t::update_action_buttons(marlin_server::State state) {
    SetButtonIconAndLabel(BtnSocket::Left, BtnRes::Settings, LabelRes::Settings);
    EnableButton(BtnSocket::Left);
    EnableButton(BtnSocket::Right);

    switch (state) {
    case marlin_server::State::Finished:
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
    const auto current = current_non_custom_status_message();
    if (!current) {
        status_text[0] = '\0';
        status_progress_available = false;
        return;
    }

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
    if (SerialPrinting::ui_mode() == SerialPrintingUiMode::legacy) {
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
    const bool show_legacy = current_page == Page::legacy;
    legacy_logo.set_visible(show_legacy);
    w_progress.set_visible(show_progress || show_message);
    w_progress_txt.set_visible(show_progress);
    w_etime_label.set_visible(show_progress);
    w_etime_value.set_visible(show_progress);
    w_time_label.set_visible(false);
    w_time_value.set_visible(false);
    time_dots.Hide();

    const bool show_status = current_page == Page::status;
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
    }

    update_page_dots();
}

void screen_printing_serial_data_t::toggle_page() {
    if (!can_toggle_pages()) {
        return;
    }

    user_selected_page = true;
    advance_page();
}

bool screen_printing_serial_data_t::can_toggle_pages() const {
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
    if (MsgBoxWarning(_("Are you sure to stop this printing?"), Responses_YesNo, 1)
        != Response::Yes) {
        return;
    }

    // abort print, disable button and wait for screen to close from marlin server
    marlin_client::print_abort();
    DisableButton(BtnSocket::Right);
}

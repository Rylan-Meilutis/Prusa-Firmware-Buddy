#include <feature/print_status_message/print_status_message_mgr.hpp>
#include <feature/print_status_message/print_status_message_formatter_buddy.hpp>
#include "screen_messages.hpp"
#include "ScreenHandler.hpp"
#include <stdlib.h>
#include "i18n.h"
#include <sound.hpp>
#include <utils/string_builder.hpp>
#include <cctype>
#include <cstring>

namespace {
bool contains_case_insensitive(const char *haystack, const char *needle) {
    for (const char *h = haystack; *h; ++h) {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np && tolower(*hp) == tolower(*np)) {
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
    while (*tp && *wp && tolower(*tp) == tolower(*wp)) {
        ++tp;
        ++wp;
    }
    if (*wp != '\0') {
        return false;
    }
    text = tp;
    return true;
}

bool is_progress_or_time_message(const char *message) {
    char *end = nullptr;
    const auto hour = strtol(message, &end, 10);
    if (end != message && hour >= 0 && hour <= 23 && *end == ':') {
        const char *minute_start = end + 1;
        const auto minute = strtol(minute_start, &end, 10);
        if (end != minute_start && minute >= 0 && minute <= 59) {
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
    }

    return strchr(message, '%')
        || contains_case_insensitive(message, "eta")
        || contains_case_insensitive(message, "etl")
        || contains_case_insensitive(message, "estimated")
        || contains_case_insensitive(message, "remaining")
        || contains_case_insensitive(message, "time left");
}
} // namespace

screen_messages_data_t::screen_messages_data_t()
    : screen_t()
    , header(this)
    , footer(this)
    , term(this, GuiDefaults::RectScreenBody.TopLeft(), &term_buff) { // Rect16(10, 28, 11 * 20, 18 * 16))
    header.SetText(_("MESSAGES"));
    ClrMenuTimeoutClose();
    ClrOnSerialClose();
}

void screen_messages_data_t::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {

    case GUI_event_t::CLICK:
    case GUI_event_t::TOUCH_SWIPE_LEFT:
    case GUI_event_t::TOUCH_SWIPE_RIGHT:
        Sound_Play(eSOUND_TYPE::ButtonEcho);
        Screens::Access()->Close();
        return;

    case GUI_event_t::LOOP:
        print_status_message().walk_history([this](const PrintStatusMessageManager::Record &msg) {
            if (msg.id <= last_message_id) {
                return true;
            }

            ArrayStringBuilder<256> buf;
            PrintStatusMessageFormatterBuddy::format(buf, msg.message);
            if (is_progress_or_time_message(buf.str())) {
                last_message_id = msg.id;
                return true;
            }
            term.Printf("%s\n", buf.str());
            last_message_id = msg.id;
            return true;
        });
        break;

    default:
        break;
    }

    screen_t::windowEvent(sender, event, param);
}

#include "screen_printing_serial.hpp"
#include "config.h"
#include "marlin_client.hpp"
#include "filament.hpp"
#include "i18n.h"
#include "ScreenHandler.hpp"
#include "odometer.hpp"
#include "config_features.h"
#include "window_icon.hpp"
#include "screen_menu_tune.hpp"
#include "img_resources.hpp"
#include <serial_printing.hpp>
#include <feature/print_status_message/print_status_message_formatter_buddy.hpp>
#include <feature/print_status_message/print_status_message_mgr.hpp>
#include <utils/string_builder.hpp>
#if ENABLED(CRASH_RECOVERY)
    #include "../Marlin/src/feature/prusa/crash_recovery.hpp"
#endif

namespace {
point_i16_t get_location() {
    constexpr int16_t term_width = width(GuiDefaults::DefaultFont) * screen_printing_serial_data_t::terminal_columns;
    constexpr auto x = GuiDefaults::RectScreenBody.Left() + (GuiDefaults::RectScreenBody.Width() - term_width) / 2;
    return point_i16_t { x, GuiDefaults::RectScreenBody.Top() };
}
} // namespace

screen_printing_serial_data_t::screen_printing_serial_data_t()
    : ScreenPrintingModel(_(caption))
    , last_tick(0)
    , connection(connection_state_t::connected)
    , term_buff()
    , term(this, get_location(), &term_buff)
    , last_state(marlin_server::State::Aborted) {
    ClrMenuTimeoutClose();
    SetOnSerialClose();

    SetButtonIconAndLabel(BtnSocket::Right, BtnRes::Disconnect, LabelRes::Stop);
}

void screen_printing_serial_data_t::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    marlin_server::State state = marlin_vars().print_state;

    if (state != last_state) {
        switch (state) {
        case marlin_server::State::Paused:
            // print is paused -> show resume button
            SetButtonIconAndLabel(BtnSocket::Middle, BtnRes::Resume, LabelRes::Resume);
            EnableButton(BtnSocket::Middle);
            break;
        case marlin_server::State::Printing:
            // print is running -> show pause button
            SetButtonIconAndLabel(BtnSocket::Middle, BtnRes::Pause, LabelRes::Pause);
            EnableButton(BtnSocket::Middle);
            break;
        default:
            // Any other state means printer pausing or resuming, so just wait for this to finish
            DisableButton(BtnSocket::Middle);
            break;
        }

        last_state = state;
    }

    if (event == GUI_event_t::LOOP) {
        print_status_message().walk_history([this](const PrintStatusMessageManager::Record &msg) {
            if (msg.id <= last_message_id) {
                return true;
            }

            ArrayStringBuilder<256> buf;
            PrintStatusMessageFormatterBuddy::format(buf, msg.message);
            term.Printf("%s\n", buf.str());
            last_message_id = msg.id;
            return true;
        });
    }

    ScreenPrintingModel::windowEvent(sender, event, param);
}

void screen_printing_serial_data_t::tuneAction() {
    Screens::Access()->Open(ScreenFactory::Screen<ScreenMenuTune>);
}

void screen_printing_serial_data_t::pauseAction() {
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
    if (MsgBoxWarning(_("Are you sure to stop this printing?"), Responses_YesNo, 1)
        != Response::Yes) {
        return;
    }

    // abort print, disable button and wait for screen to close from marlin server
    marlin_client::print_abort();
    DisableButton(BtnSocket::Right);
}

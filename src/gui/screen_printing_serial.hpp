// screen_printing_serial.hpp
#pragma once
#include "ScreenPrintingModel.hpp"
#include "gui.hpp"
#include "window_header.hpp"
#include "status_footer.hpp"
#include "window_text.hpp"
#include <array>
#include "window.hpp"
#include "window_term.hpp"

class screen_printing_serial_data_t : public ScreenPrintingModel {
    static constexpr const char *caption = N_("SERIAL PRINTING  (MESSAGES)");

    int last_tick;
    enum class connection_state_t { connected,
        disconnect,
        disconnect_ask,
        disconnecting,
        disconnected };
    connection_state_t connection;

public:
    static constexpr size_t terminal_columns = 30;
    static constexpr size_t terminal_rows = 8;

    screen_printing_serial_data_t();

protected:
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    virtual void stopAction() override;
    virtual void pauseAction() override;
    virtual void tuneAction() override;
    term_buff_t<terminal_columns, terminal_rows> term_buff;
    window_term_t term;
    uint32_t last_message_id = 0;
    marlin_server::State last_state;
};

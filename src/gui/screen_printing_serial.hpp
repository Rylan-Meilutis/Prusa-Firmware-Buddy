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
#include "window_print_progress.hpp"
#include "print_time_module.hpp"
#include <window_progress.hpp>

class screen_printing_serial_data_t : public ScreenPrintingModel {
    static constexpr const char *caption = N_("SERIAL PRINTING");

    int last_tick;
    enum class connection_state_t { connected,
        disconnect,
        disconnect_ask,
        disconnecting,
        disconnected };
    connection_state_t connection;

public:
    static constexpr size_t terminal_columns = HAS_MINI_DISPLAY() ? 20 : 30;
    static constexpr size_t terminal_rows = 8;

    screen_printing_serial_data_t();

protected:
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    enum class Page {
        progress,
        status,
        message,
    };

    enum class TimeItem {
        remaining_time,
        end_time,
        time_to_change,
        time_since_start,
        _count,
    };

    virtual void stopAction() override;
    virtual void pauseAction() override;
    virtual void tuneAction() override;
    void update_progress();
    bool time_item_available(TimeItem item) const;
    TimeItem next_time_item(TimeItem item) const;
    TimeItem previous_time_item(TimeItem item) const;
    TimeItem first_time_item() const;
    TimeItem last_time_item() const;
    void update_status();
    void update_messages();
    void update_action_buttons(marlin_server::State state);
    void set_page(Page page);
    void toggle_page();
    void advance_page();
    void retreat_page();
    void update_page_dots();
    bool can_toggle_pages() const;
    size_t page_count() const;
    size_t current_page_index() const;
    bool status_page_available() const;
    bool message_page_available() const;
    term_buff_t<terminal_columns, terminal_rows> term_buff;
    window_term_t term;
    WindowPrintProgress w_progress;
    WindowNumbPrintProgress w_progress_txt;
    window_text_t w_etime_label;
    window_text_t w_etime_value;
    window_text_t w_time_label;
    window_text_t w_time_value;
    window_text_t w_status_label;
    window_text_t w_status_value;
    WindowProgressBar w_status_progress;
    window_text_t w_message_label;
    window_text_t w_message_value;
    WindowProgressCircles time_dots;
    WindowProgressCircles page_dots;
    std::array<char, 32> w_etime_value_buffer {};
    std::array<char, 32> w_time_value_buffer {};
    std::array<char, 256> status_text {};
    std::array<char, 64> status_value_text {};
    std::array<char, 256> message_text {};
    Page current_page = Page::progress;
    TimeItem current_time_item = TimeItem::remaining_time;
    uint32_t last_page_switch_s = 0;
    bool user_selected_page = false;
    bool status_progress_available = false;
    uint32_t last_message_id = 0;
    marlin_server::State last_state;
    bool lock_buttons_applied = false;
};

#pragma once

#include "gui.hpp"
#include "screen.hpp"
#include "window_header.hpp"
#include "window_text.hpp"
#include "radio_button.hpp"

class ScreenEndFiltration : public screen_t {
    window_header_t header;
    window_text_t description;
    RadioButton radio;

public:
    ScreenEndFiltration();

protected:
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};

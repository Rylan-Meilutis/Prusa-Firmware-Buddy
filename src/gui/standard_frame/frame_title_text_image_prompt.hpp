#pragma once

#include <client_response.hpp>
#include <footer_line.hpp>
#include <radio_button_fsm.hpp>
#include <window_frame.hpp>
#include <window_icon.hpp>

/**
 * Standard layout frame.
 * Contains:
 * - Orange left-aligned title
 * - Gray line (visible if title is not empty)
 * - Text on the left side
 * - Image on the right side
 * - A FSM radio
 */
class FrameTitleTextImagePrompt {
public:
    FrameTitleTextImagePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &txt_title, const string_view_utf8 &txt_info, const img::Resource *img_res);

    void add_footer(FooterLine &footer);

protected:
    window_text_t title;
    BasicWindow title_line;
    window_text_t info;
    window_icon_t icon;
    RadioButtonFSM radio;
};

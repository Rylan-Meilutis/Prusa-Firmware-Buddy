#pragma once

#include <client_response.hpp>
#include <footer_line.hpp>
#include <radio_button_fsm.hpp>
#include <window_frame.hpp>
#include <window_icon.hpp>

/**
 * Standard layout frame.
 * Contains:
 * - Text on the left side (alignment can be changed)
 * - Image on the right side
 * - A FSM radio
 */
class FrameTextImagePrompt {
public:
    FrameTextImagePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &txt_info, const img::Resource *img_res);

    void add_footer(FooterLine &footer);

protected:
    window_text_t info;
    window_icon_t icon;
    RadioButtonFSM radio;
};

#pragma once

#include "screen_fsm.hpp"
#include "radio_button_fsm.hpp"
#include "window_text.hpp"
#include "window_wizard_progress.hpp"

class ScreenPACalibrationProgress final : public ScreenFSM {
public:
    ScreenPACalibrationProgress();

protected:
    void create_frame() final;
    void destroy_frame() final {}
    void update_frame() final;

private:
    window_text_t stage_text;
    window_text_t tool_text;
    window_wizard_progress_t progress;
    RadioButtonFSM radio;
    std::array<char, 128> tool_buffer {};
};

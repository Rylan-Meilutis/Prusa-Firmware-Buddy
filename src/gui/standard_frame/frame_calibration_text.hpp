/// @file
#pragma once

#include <gui/frame_calibration_common.hpp>
#include <client_response.hpp>
#include <radio_button_fsm.hpp>
#include <window_frame.hpp>

class FrameCalibrationText : public standard_frame_without_radio::FrameText {
protected:
    FrameCalibrationText(window_frame_t *parent, FSMAndPhase fsm_phase, string_view_utf8 txt, const Rect16::Top_t top)
        : FrameText(parent, txt, top)
        , radio(parent, WizardDefaults::RectRadioButton(0), fsm_phase) {
        parent->CaptureNormalWindow(radio);
    }

private:
    RadioButtonFSM radio;
};

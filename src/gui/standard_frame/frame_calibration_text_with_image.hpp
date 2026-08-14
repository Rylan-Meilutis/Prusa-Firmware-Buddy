/// @file
#pragma once

#include <gui/frame_calibration_common.hpp>
#include <client_response.hpp>
#include <radio_button_fsm.hpp>
#include <window_frame.hpp>

class FrameCalibrationTextWithImage : public standard_frame_without_radio::FrameTextWithImage {

protected:
    FrameCalibrationTextWithImage(window_frame_t *parent, FSMAndPhase fsm_phase, string_view_utf8 txt, Rect16::Top_t top, const img::Resource *icon_res, uint16_t icon_width)
        : FrameTextWithImage(parent, txt, top, icon_res, icon_width)
        , radio(parent, WizardDefaults::RectRadioButton(0), fsm_phase) {
        parent->CaptureNormalWindow(radio);
    }

private:
    RadioButtonFSM radio;
};

#include "screen_pa_calibration_progress.hpp"

#include "frame_calibration_common.hpp"
#include <common/marlin_server_types/client_response.hpp>
#include <feature/extrusion_calibration.hpp>

namespace {
constexpr const char *header_text = N_("PRESSURE ADVANCE");

PhasesPressureAdvanceCalibration phase_of(const fsm::BaseData &data) {
    return GetEnumFromPhaseIndex<PhasesPressureAdvanceCalibration>(data.GetPhase());
}

const char *phase_text(const PhasesPressureAdvanceCalibration phase) {
    switch (phase) {
    case PhasesPressureAdvanceCalibration::heating: return N_("Heating nozzle");
    case PhasesPressureAdvanceCalibration::homing: return N_("Homing");
    case PhasesPressureAdvanceCalibration::probing: return N_("Probing anchor area");
    case PhasesPressureAdvanceCalibration::measuring: return N_("Measuring nozzle pressure");
    case PhasesPressureAdvanceCalibration::computing: return N_("Computing the best value");
    case PhasesPressureAdvanceCalibration::cleanup: return N_("Cleaning nozzle");
    case PhasesPressureAdvanceCalibration::result: return N_("Pressure advance result");
    case PhasesPressureAdvanceCalibration::complete: return N_("Calibration complete");
    case PhasesPressureAdvanceCalibration::failed: return N_("Calibration stopped; fallback applied");
    }
    return N_("Calibrating");
}
} // namespace

ScreenPACalibrationProgress::ScreenPACalibrationProgress()
    : ScreenFSM(header_text, rect_screen)
    , stage_text(this, rect_frame_top, is_multiline::yes, is_closed_on_click_t::no)
    , tool_text(this, rect_frame_bottom, is_multiline::yes, is_closed_on_click_t::no)
    , progress(this, Rect16::Top_t { 100 })
    // Reserve one footer row. The generic calibration rectangle uses the
    // bottom row when passed 0, which made the footer cover the Abort button.
    , radio(this, WizardDefaults::RectRadioButton(1), PhasesPressureAdvanceCalibration::measuring)
    , footer(this) {
    stage_text.SetAlignment(Align_t::Center());
    tool_text.SetAlignment(Align_t::Center());
    CaptureNormalWindow(radio);
}

void ScreenPACalibrationProgress::create_frame() {
    radio.Change(phase_of(fsm_base_data));
    update_frame();
}

void ScreenPACalibrationProgress::update_frame() {
    const auto data = fsm_base_data.GetData();
    stage_text.SetText(_(phase_text(phase_of(fsm_base_data))));
    if (phase_of(fsm_base_data) == PhasesPressureAdvanceCalibration::result) {
        if (data[2] == 0xff && data[3] == 0xff) {
            size_t used = 0;
            for (uint8_t slot = 0; slot < buddy::extrusion_calibration::max_logical_filaments && used < tool_buffer.size(); ++slot) {
                if (!(data[1] & (1u << slot))) continue;
                if (const auto *result = buddy::extrusion_calibration::job_result(slot)) {
                    used += snprintf(tool_buffer.data() + used, tool_buffer.size() - used, "%sT%u: %.3f",
                        used ? "  " : "", unsigned(slot + 1), static_cast<double>(result->pressure_advance));
                }
            }
        } else {
            const unsigned milli = unsigned(data[2]) | (unsigned(data[3]) << 8);
            snprintf(tool_buffer.data(), tool_buffer.size(), "Tool %u: PA %.3f", unsigned(data[1]), milli / 1000.0);
        }
    } else {
        snprintf(tool_buffer.data(), tool_buffer.size(), "Tool / slot %u", unsigned(data[1]));
    }
    tool_text.SetText(string_view_utf8::MakeRAM(tool_buffer.data()));
    progress.set_progress_percent(data[0]);
    stage_text.Invalidate();
    tool_text.Invalidate();
}

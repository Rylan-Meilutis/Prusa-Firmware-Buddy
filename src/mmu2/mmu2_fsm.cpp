#include "mmu2_fsm.hpp"
#include "pause_stubbed.hpp"
#include "mmu2_error_converter.h"
#include "fsm_loadunload_type.hpp"
#include <serial_printing.hpp>

LOG_COMPONENT_REF(MMU2);

/// Convenience prefix +op for enum values, works same as to_underlying
/// It's being used a lot in this file so writing to_underlying everywhere would be a mess
template <typename T>
static constexpr auto operator+(T e) noexcept
    -> std::enable_if_t<std::is_enum<T>::value, std::underlying_type_t<T>> {
    return static_cast<std::underlying_type_t<T>>(e);
}

namespace MMU2 {

static uint8_t last_rme_progress_code = 0xff;
static uint8_t last_rme_progress_percent = 0xff;

static constexpr PhasesLoadUnload ProgressCodeToPhasesLoadUnload(RawProgressCode pc) {
    // Using +op prefix operator defined on the beginning of this file, works same as to_underlying

    switch (pc) {
    case +ProgressCode::EngagingIdler:
        return PhasesLoadUnload::MMU_EngagingIdler;
    case +ProgressCode::DisengagingIdler:
        return PhasesLoadUnload::MMU_DisengagingIdler;
    case +ProgressCode::UnloadingToFinda:
        return PhasesLoadUnload::MMU_UnloadingToFinda;
    case +ProgressCode::UnloadingToPulley:
        return PhasesLoadUnload::MMU_UnloadingToPulley;
    case +ProgressCode::FeedingToFinda:
        return PhasesLoadUnload::MMU_FeedingToFinda;
    case +ProgressCode::FeedingToBondtech:
        return PhasesLoadUnload::MMU_FeedingToBondtech;
    case +ProgressCode::FeedingToNozzle:
        return PhasesLoadUnload::MMU_FeedingToNozzle;
    case +ProgressCode::AvoidingGrind:
        return PhasesLoadUnload::MMU_AvoidingGrind;
    case +ProgressCode::OK:
    case +ProgressCode::FinishingMoves:
        return PhasesLoadUnload::MMU_FinishingMoves;
    case +ProgressCode::ERRDisengagingIdler:
        return PhasesLoadUnload::MMU_ERRDisengagingIdler;
    case +ProgressCode::ERREngagingIdler:
        return PhasesLoadUnload::MMU_ERREngagingIdler;
        //    case ProgressCode::ERRWaitingForUser: return PhasesLoadUnload::MMU_ErrWaitForUser; // this never happens, instead the MMU reports the error
        //    case ProgressCode::ERRInternal:
    case +ProgressCode::ERRHelpingFilament:
        return PhasesLoadUnload::MMU_ERRHelpingFilament;
        //    case ProgressCode::ERRTMCFailed:
    case +ProgressCode::UnloadingFilament:
        return PhasesLoadUnload::MMU_UnloadingFilament;
    case +ProgressCode::LoadingFilament:
        return PhasesLoadUnload::MMU_LoadingFilament;
    case +ProgressCode::SelectingFilamentSlot:
        return PhasesLoadUnload::MMU_SelectingFilamentSlot;
    case +ProgressCode::PreparingBlade:
        return PhasesLoadUnload::MMU_PreparingBlade;
    case +ProgressCode::PushingFilament:
        return PhasesLoadUnload::MMU_PushingFilament;
    case +ProgressCode::PerformingCut:
        return PhasesLoadUnload::MMU_PerformingCut;
    case +ProgressCode::ReturningSelector:
        return PhasesLoadUnload::MMU_ReturningSelector;
    case +ProgressCode::ParkingSelector:
        return PhasesLoadUnload::MMU_ParkingSelector;
    case +ProgressCode::EjectingFilament:
        return PhasesLoadUnload::MMU_EjectingFilament;
    case +ProgressCode::RetractingFromFinda:
        return PhasesLoadUnload::MMU_RetractingFromFinda;
    case +ProgressCode::Homing:
        return PhasesLoadUnload::MMU_Homing;
    case +ProgressCode::MovingSelector:
        return PhasesLoadUnload::MMU_MovingSelector;
    case +ProgressCode::FeedingToFSensor:
        return PhasesLoadUnload::MMU_FeedingToFSensor;
    case +ProgressCode::HWTestBegin:
        return PhasesLoadUnload::MMU_HWTestBegin;
    case +ProgressCode::HWTestIdler:
        return PhasesLoadUnload::MMU_HWTestIdler;
    case +ProgressCode::HWTestSelector:
        return PhasesLoadUnload::MMU_HWTestSelector;
    case +ProgressCode::HWTestPulley:
        return PhasesLoadUnload::MMU_HWTestPulley;
    case +ProgressCode::HWTestCleanup:
        return PhasesLoadUnload::MMU_HWTestCleanup;
    case +ProgressCode::HWTestExec:
        return PhasesLoadUnload::MMU_HWTestExec;
    case +ProgressCode::HWTestDisplay:
        return PhasesLoadUnload::MMU_HWTestDisplay;
    case +ProgressCode::ErrHwTestFailed:
        return PhasesLoadUnload::MMU_ErrHwTestFailed;

    case +ExtendedProgressCode::WaitingForTemperature:
        return PhasesLoadUnload::WaitingTemp_unstoppable;
    case +ExtendedProgressCode::UnloadingFromExtruder:
        return PhasesLoadUnload::Unloading_unstoppable;
    case +ExtendedProgressCode::LoadingToNozzle:
        return PhasesLoadUnload::Loading_unstoppable;
    case +ExtendedProgressCode::Ramming:
        return PhasesLoadUnload::Ramming_unstoppable;

    default:
        assert(0);

        // Cannot be ERR_WaitingForUser, because with that state we expect specific data to be passed
        // and if the data is malformed we get a bsod
        return PhasesLoadUnload::MMU_Homing;
    }
}

static constexpr const char *ProgressCodeToRmeState(RawProgressCode pc) {
    switch (pc) {
    case +ProgressCode::EngagingIdler: return "engaging_idler";
    case +ProgressCode::DisengagingIdler: return "disengaging_idler";
    case +ProgressCode::UnloadingToFinda: return "unloading_to_finda";
    case +ProgressCode::UnloadingToPulley: return "unloading_to_pulley";
    case +ProgressCode::FeedingToFinda: return "feeding_to_finda";
    case +ProgressCode::FeedingToBondtech: return "feeding_to_extruder";
    case +ProgressCode::FeedingToNozzle: return "feeding_to_nozzle";
    case +ProgressCode::AvoidingGrind: return "avoiding_grind";
    case +ProgressCode::OK: return "complete";
    case +ProgressCode::FinishingMoves: return "finishing_moves";
    case +ProgressCode::ERRDisengagingIdler: return "error_disengaging_idler";
    case +ProgressCode::ERREngagingIdler: return "error_engaging_idler";
    case +ProgressCode::ERRHelpingFilament: return "helping_filament";
    case +ProgressCode::UnloadingFilament: return "unloading_filament";
    case +ProgressCode::LoadingFilament: return "loading_filament";
    case +ProgressCode::SelectingFilamentSlot: return "selecting_slot";
    case +ProgressCode::PreparingBlade: return "preparing_blade";
    case +ProgressCode::PushingFilament: return "pushing_filament";
    case +ProgressCode::PerformingCut: return "cutting_filament";
    case +ProgressCode::ReturningSelector: return "returning_selector";
    case +ProgressCode::ParkingSelector: return "parking_selector";
    case +ProgressCode::EjectingFilament: return "ejecting_filament";
    case +ProgressCode::RetractingFromFinda: return "retracting_from_finda";
    case +ProgressCode::Homing: return "homing";
    case +ProgressCode::MovingSelector: return "moving_selector";
    case +ProgressCode::FeedingToFSensor: return "feeding_to_filament_sensor";
    case +ProgressCode::HWTestBegin: return "hardware_test_begin";
    case +ProgressCode::HWTestIdler: return "hardware_test_idler";
    case +ProgressCode::HWTestSelector: return "hardware_test_selector";
    case +ProgressCode::HWTestPulley: return "hardware_test_pulley";
    case +ProgressCode::HWTestCleanup: return "hardware_test_cleanup";
    case +ProgressCode::HWTestExec: return "hardware_test_execute";
    case +ProgressCode::HWTestDisplay: return "hardware_test_results";
    case +ProgressCode::ErrHwTestFailed: return "hardware_test_failed";
    case +ExtendedProgressCode::WaitingForTemperature: return "waiting_for_temperature";
    case +ExtendedProgressCode::UnloadingFromExtruder: return "unloading_from_extruder";
    case +ExtendedProgressCode::LoadingToNozzle: return "loading_to_nozzle";
    case +ExtendedProgressCode::Ramming: return "ramming";
    default: return "unknown";
    }
}

bool Fsm::IsActive() const {
    return created_this || Pause::IsFsmActive();
};

// wait until conditions are right
// no fsm is open
// idle loop can cause to call MMU and it can cause to call idle
// mmu fsm lock??
void Fsm::Loop() {
    if (!IsActive()) {
        if (auto rep = Fsm::Instance().reporter.PeekReport()) {
            if (std::holds_alternative<MMU2::ErrorData>(*rep)) {
                Activate(); // just open now, next loop will handle error
                // TODO we might need to modify pause.cpp tho handle this state
            } else {
                // We cannot report due to a closed FSM, this can happen
                // (besides other errorneous states) e.g. when the FSM is
                // closed before the last (valid) report is proceccessed by
                // this Loop
                Fsm::Instance().reporter.ConsumeReport();
            }
        }
        return;
    }

    std::optional<Reporter::Report> report = Fsm::Instance().reporter.ConsumeReport();
    if (!report) {
        return;
    }

    std::visit([&](auto &&r) -> void {
        using T = std::decay_t<decltype(r)>;
        if constexpr (std::is_same_v<T, ProgressData>) {
            progressManager.ProcessReport(r);

            log_debug(MMU2, "Report progress =%u", static_cast<unsigned>(r.rawProgressCode));

            const FSMLoadUnloadData data { .mode = progressManager.GetLoadUnloadMode(), .progress = progressManager.GetProgressPercentage() };
            marlin_server::fsm_change(ProgressCodeToPhasesLoadUnload(progressManager.GetProgressCode()), fsm::serialize_data(data));
            const char *status = "MMU filament operation";
            switch (data.mode) {
            case LoadUnloadMode::Load: status = "MMU loading filament"; break;
            case LoadUnloadMode::Unload: status = "MMU unloading filament"; break;
            case LoadUnloadMode::Change: status = "MMU changing filament"; break;
            case LoadUnloadMode::Cut: status = "MMU cutting filament"; break;
            case LoadUnloadMode::Eject: status = "MMU ejecting filament"; break;
            case LoadUnloadMode::Test: status = "MMU testing filament path"; break;
            case LoadUnloadMode::Purge: status = "MMU purging filament"; break;
            case LoadUnloadMode::FilamentStuck: status = "MMU filament stuck"; break;
            }
            const auto raw_progress_code = static_cast<uint8_t>(r.rawProgressCode);
            const bool useful_rme_progress = last_rme_progress_percent == 0xff
                || data.progress >= last_rme_progress_percent + 5
                || data.progress + 5 <= last_rme_progress_percent;
            if (raw_progress_code != last_rme_progress_code || useful_rme_progress) {
                const char *rme_state = ProgressCodeToRmeState(r.rawProgressCode);
                SerialPrinting::notify_progress("mmu", rme_state, rme_state, status, data.progress);
                last_rme_progress_code = raw_progress_code;
                last_rme_progress_percent = data.progress;
            }
            SerialPrinting::notify_status(status, data.progress);

        } else if constexpr (std::is_same_v<T, ErrorData>) {
            if (r.errorCode == ErrorCode::MMU_NOT_RESPONDING) {
                return;
            }

            log_debug(MMU2, "Report error =%u", static_cast<unsigned>(r.errorCode));
            SerialPrinting::notify_error("mmu", "mmu_error", ConvertMMUErrorCode(r.errorCode).err_title);
            SerialPrinting::notify_workflow("mmu", "waiting", "MMU action required");
            marlin_server::fsm_change(
                PhasesLoadUnload::MMU_ERRWaitingForUser,
                fsm::serialize_data(&ConvertMMUErrorCode(r.errorCode)));
        }
    },
        *report);
}

bool Fsm::Activate() {
    if (Pause::IsFsmActive()) {
        return false; // FSM not ours, avoid setting the created_this flag
    }

    if (created_this) {
        return false; // already created by us, skip repeated fsm_create
    }

    created_this = true;
    last_rme_progress_code = 0xff;
    last_rme_progress_percent = 0xff;
    SerialPrinting::notify_workflow("mmu", "open", "MMU operation started", 0);
    marlin_server::fsm_create(PhasesLoadUnload::MMUDummyStartNoAttention);
    return true;
}

bool Fsm::Deactivate() {
    if (Pause::IsFsmActive()) {
        return false; // FSM not ours, avoid killing the FSM even if created_this == true
    }

    if (!created_this) {
        return false; // not created at all, avoid caling fsm_destroy
    }

    created_this = false;
    marlin_server::fsm_destroy(ClientFSM::Load_unload);
    SerialPrinting::notify_workflow("mmu", "closed", "MMU operation complete", 100);
    return true;
}

} // namespace MMU2

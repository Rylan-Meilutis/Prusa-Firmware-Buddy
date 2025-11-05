#pragma once

#include <screen_fsm.hpp>
#include <fsm/filament_change_phases.hpp>

class DialogLoadUnload : public DialogFSM {
public:
    DialogLoadUnload(fsm::BaseData data);
    ~DialogLoadUnload();

protected:
    void create_frame() override;
    void destroy_frame() override;
    void update_frame() override;

private:
    inline PhasesLoadUnload get_phase() const {
        return GetEnumFromPhaseIndex<PhasesLoadUnload>(fsm_base_data.GetPhase());
    }
};

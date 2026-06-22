/// @file
#include "indx_hotend.hpp"

#include <puppies/INDX.hpp>
#include <common/aggregate_arity.hpp>
#include <feature/indx_hotend_temp_model/hotend_temp_model.hpp>

void IndxHotend::handle_nozzle_target_change() {
    BaseHotend::handle_nozzle_target_change();
    buddy::puppies::indx.set_hotend_target_temp(nozzle_target_temp());

    // Temp change indicates possible filament parameters change, recompute
    buddy::hotend_temp_model().update_filament_params();
}

void IndxHotend::start_heating() {
    assert_thermally_managed_invariant(NoTool {});
    is_thermally_managed_ = true;
    // Bind the thermal model to this tool's managed session.
    buddy::hotend_temp_model().set_tool(tool_.currently_selected_virtual_tool());
    // Sync now (manage() didn't run while parked) so the first manage() won't re-fire on a park-time reset.
    last_head_reset_count_ = buddy::puppies::indx.get_reset_counter();
    handle_nozzle_target_change();
}

void IndxHotend::stop_heating() {
    // target doesn't change so the next heating can heat to original target
    buddy::puppies::indx.set_hotend_target_temp(0);
    // tool is still physically on head, but it is no longer thermally managed
    is_thermally_managed_ = false;

    nozzle_temp_ = 15; // INDX_TODO: Fix mintemp so that here can be temperature_invalid
    nozzle_heater_pwm_ = 0;

    // No longer thermally managed: unbind the thermal model, zeroing its head compensation.
    buddy::hotend_temp_model().set_tool(NoTool {});

    assert_thermally_managed_invariant(NoTool {});
}

void IndxHotend::assert_thermally_managed_invariant(std::variant<PhysicalToolIndex, NoTool> expected_managed) {
    for (auto t : PhysicalToolIndex::all()) {
        if (stdext::holds_value(expected_managed, t)) {
            continue;
        }
        if (IndxHotend::indx_tool(t).hotend().is_thermally_managed()) {
            bsod_unreachable();
        }
    }
}

void IndxHotend::manage() {
    assert(is_thermally_managed());

    // self-paced internally, so calling it each manage() tick is fine.
    buddy::hotend_temp_model().step();

    const auto reset_count = buddy::puppies::indx.get_reset_counter();
    const bool head_got_reset = (reset_count != last_head_reset_count_);
    last_head_reset_count_ = reset_count;

    nozzle_temp_ = buddy::puppies::indx.get_hotend_temp_compensated();

    if (head_got_reset) {
        // Act as if nozzle target temp changed
        // This resets all the safety guards (heater watch, thermal runaway, ...)
        // to prevent them from semi-falsely triggering when the indx head reset (which caused a temporary heating dropout)
        handle_nozzle_target_change();
        // Re-init the thermal model so it doesn't run against the reset discontinuity
        buddy::hotend_temp_model().reset();
    }

    BaseHotend::manage();
}

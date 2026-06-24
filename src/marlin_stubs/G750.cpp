#include "PrusaGcodeSuite.hpp"

#include <config_store/store_instance.hpp>
#include <gcode/gcode_parser.hpp>
#include <module/prusa/toolchanger_utils.h>
#include <raii/scope_guard.hpp>
#include <module/planner.h>
#include <filament.hpp>
#include <tool_index.hpp>

void PrusaGcodeSuite::G750() {
    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    MachinePosXYZE target = current_machine_position();

    if (auto x = p.option<float>('X')) {
        target.x = *x + X_NOZZLE_CLEANER_ORIGIN + config_store().nozzle_cleaner_x_origin_offset.get();
    }
    if (auto y = p.option<float>('Y')) {
        target.y = *y + Y_NOZZLE_CLEANER_ORIGIN + config_store().nozzle_cleaner_y_origin_offset.get();
    }
    if (auto e = p.option<float>('E')) {
        target.e += *e;
    }

    float feedrate = p.option<float>('F').value_or(PrusaToolChangerUtils::TRAVEL_MOVE_MM_S);

    // L = adjust the feedrate based on the loaded filament's properties.
    if (p.has_option('L')) {
        feedrate = adjust_feedrate_for_filament(feedrate, FilamentType::for_tool_heuristic(VirtualToolIndex::currently_selected()));
    }

    // Use machine coordinates - wastebin is outside of MBL area, applying MBL would do funny stuff.
    line_to_machine_pos(target, feedrate, { .ignore_e_factor = true });

    // A = asynchronous - do not wait for the planner to finish the move
    if (!p.has_option('A')) {
        planner.synchronize();
    }
}

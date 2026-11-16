; RME INDX: replace Orca Machine G-code > Change filament G-code.
; Opt-in: false = entry only; true = entry + exit. Qualify pellet retention first.
; No timed dwell. M109 remains the required temperature-readiness wait.
{local rme_eject_on_exit = false}
{local speed_tc = min(travel_speed, 350.0) * 60}

{if enable_prime_tower == false}
  ; Change Tool[previous_extruder] -> Tool[next_extruder] (layer [layer_num])
  {if is_nil(retract_length_toolchange[current_extruder])}
    G1 E-[retraction_length[current_extruder]] F{retraction_speed[current_extruder] * 60}
  {endif}
  G1 F{speed_tc}
  M204 S7000
  {if not (layer_num < 0 and next_extruder == initial_tool)}
    G27 W3 Z{z_hop[current_extruder]} P2 R{retract_toolchange} V{retraction_speed[current_extruder]} A{travel_slope[current_extruder]}
  {endif}
  ; Native T parks the previous tool and picks the next; no intermediate P0 detour.
  T{next_extruder} S1 L0 D0
  ; Orca emits the evaluated filament-start G-code after this tool change.
  {local target_temp = nozzle_temperature[next_extruder]}
  {if layer_z <= (initial_layer_print_height + 0.0001)}
    {target_temp = nozzle_temperature_initial_layer[next_extruder]}
  {endif}
  M104 S{target_temp}
  G12 S90 ; enter cleaner while the selected tool heats
  G12 S1 ; native sequence already drains the planner
  G12 S30 F1 ; eject leftover pellet on entry
  M106 S{255 / 100 * fan_max_speed[next_extruder]}
  M109 S{target_temp} ; retain thermal readiness before any extrusion
  M906 P1 ; increased E current for prime/purge
  M83
  {if is_nil(retract_length_toolchange[next_extruder])}
    G1 E{retract_toolchange + retraction_length[next_extruder]} F{20 * 60}
  {else}
    G1 E{retract_toolchange + retract_length_toolchange[next_extruder]} F{20 * 60}
  {endif}
  M400 ; complete fast reinsertion before changing the purge acceleration
  M201 E{default_acceleration * line_width * layer_height / (3.14 * filament_diameter[next_extruder] * filament_diameter[next_extruder] / 4)}
  ;EXCLUDE_E_START
  G1 E3.4 F{0.6 * 60 * filament_max_volumetric_speed[next_extruder] / 3.14 / filament_diameter[next_extruder]}
  ;EXCLUDE_E_END
  ;FLUSH_START
  G91 ; use relative coordinates
  M83 ; extruder relative mode
  G1 E{(0.4 * filament_minimal_purge_on_wipe_tower[current_extruder]) / (3.14 * filament_diameter[next_extruder] * filament_diameter[next_extruder] / 4)} F{0.8 * 60 * filament_max_volumetric_speed[next_extruder] / 3.14 / filament_diameter[next_extruder]}
  G1 Y-1.5 E{(0.3 * filament_minimal_purge_on_wipe_tower[current_extruder]) / (3.14 * filament_diameter[next_extruder] * filament_diameter[next_extruder] / 4)} F{0.8 * 60 * filament_max_volumetric_speed[next_extruder] / 3.14 / filament_diameter[next_extruder]}
  G1 Y1.5 E{(0.3 * filament_minimal_purge_on_wipe_tower[current_extruder]) / (3.14 * filament_diameter[next_extruder] * filament_diameter[next_extruder] / 4)} F{min(250, 60*filament_max_volumetric_speed[next_extruder]/3.14/filament_diameter[next_extruder]/filament_diameter[next_extruder]*4)}
  G90 ; use absolute coordinates
  M83 ; extruder relative mode
  ;FLUSH_END
  M400 ; finish purge BEFORE restoring acceleration and physical motor current
  M201 E[machine_max_acceleration_e]
  M906 P0 ; restore E current only after the purge completes
  G1 E-{1.5 * retraction_length[next_extruder]} F{retraction_speed[next_extruder] * 60}
  {e_retracted[next_extruder] = 1.5 * retraction_length[next_extruder]}
  G1 F{speed_tc}
  ; Planner ordering finishes the retract before this native blocking wipe.
  G12 S1 ; retain nozzle wipe even when leaving the pellet until next entry
  {if rme_eject_on_exit}
    G12 S30 F1 ; explicit Entry + Exit overrides the LCD entry-only filter
  {endif}
  G12 S91 ; native safe exit, already synchronizes
  G0 Z{layer_z + 1} ; retain clearance
  {if layer_z <= (initial_layer_print_height + (close_fan_the_first_x_layers[next_extruder] - 1) * layer_height + 0.0001)}
    M106 S0
  {endif}
  G1 F{speed_tc}
{else}
  {if previous_extruder != -1}
    ; Wipe tower enabled: use the same entry-eject policy as the no-tower path.
    ; Change Tool[previous_extruder] -> Tool[next_extruder] (layer [layer_num])
    {if is_nil(retract_length_toolchange[current_extruder])}
      G1 E-{retraction_length[current_extruder]} F{retraction_speed[current_extruder] * 60}
    {endif}
    G1 F{speed_tc}
    M204 S7000
    G27 W3 Z{z_hop[current_extruder]} P2 R{retract_toolchange} V{retraction_speed[current_extruder]} A{travel_slope[current_extruder]}
    T{next_extruder} S1 L0 D0
    {local target_temp = nozzle_temperature[next_extruder]}
    {if layer_num == 0}
      {target_temp = nozzle_temperature_initial_layer[next_extruder]}
    {endif}
    M104 S{target_temp}
    G12 S90 ; enter cleaner while the selected tool heats
    G12 S1 ; wipe before clearing the retained pellet
    G12 S30 F1 ; entry eject replaces the old six inline G750 shakes
    M109 S{target_temp} ; retain thermal readiness before any extrusion
    M83
    {if is_nil(retract_length_toolchange[next_extruder])}
      G1 E{retract_toolchange + retraction_length[next_extruder]} F{20 * 60}
    {else}
      G1 E{retract_toolchange + retract_length_toolchange[next_extruder]} F{20 * 60}
    {endif}
    ;EXCLUDE_E_START
    G1 E1.2 F{0.6 * 60 * filament_max_volumetric_speed[next_extruder] / 3.14 / filament_diameter[next_extruder]}
    ;EXCLUDE_E_END
    G1 E-{1.5 * retraction_length[next_extruder]} F{retraction_speed[next_extruder] * 60}
    {e_retracted[next_extruder] = 1.5 * retraction_length[next_extruder]}
    ; Native wipe executes after the queued purge/retract and drains the planner.
    G12 S1
    {if rme_eject_on_exit}
      G12 S30 F1 ; explicit Entry + Exit
    {endif}
    G12 S91 ; native safe exit
    G0 Z{layer_z + 0.5} ; retain clearance
    G1 F{speed_tc}
  {endif}
{endif}

; CORE One + MMU RME start G-code with automatic pressure-advance calibration

; --- Printer Initialization and Checks ---
M75 ; start print timer / serial print mode

M17 ; enable steppers
M151 W255 D0 T500
M862.1 P[nozzle_diameter] A{(printer_notes=~/.*ABRASIVE_NOZZLE.*/ ? 1 : 0)} F{(printer_notes=~/.*HF_NOZZLE.*/ ? 1 : 0)} ; nozzle check
M862.3 P "COREONE" ; printer model check
M862.5 P2 ; g-code level check
M862.6 P"Input shaper" ; FW feature check
M115 U6.3.0+10073

; setup MMU
M708 A0x0b X5   ; extra load distance
M708 A0x22 X550 ; bowden length
M708 A0x0d X140 ; unload feedrate
M708 A0x11 X140 ; load feedrate
M708 A0x14 X20  ; slow feedrate
M708 A0x1e X12  ; pulley current to ~200mA

; --- Print Area and Coordinate System ---

M555 X{(min(print_bed_max[0], first_layer_print_min[0] + 32) - 32)} Y{(max(0, first_layer_print_min[1]) - 4)} W{((min(print_bed_max[0], max(first_layer_print_min[0] + 32, first_layer_print_max[0])))) - ((min(print_bed_max[0], first_layer_print_min[0] + 32) - 32))} H{((first_layer_print_max[1])) - ((max(0, first_layer_print_min[1]) - 4))} ; define print area

G90 ; use absolute coordinates
M83 ; extruder in relative mode

; --- Preparation ---

{if chamber_temperature[initial_tool] > 35}
; we need to preheat the chamber
M140 S115 ; set bed temp for chamber heating
{else}
; just set the selected bed temp otherwise
M140 S[first_layer_bed_temperature] ; set bed temp
{endif}

M109 R{((filament_notes[0]=~/.*HT_MBL10.*/) ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0] == "FLEX") ? 210 : 170)} ; wait for temp

M84 E ; turn off E motor

G28 ; home all axes without mesh bed leveling

; --- Chamber Temperature Control ---
{if chamber_temperature[0] > 35}
M870 C ; close top vent for heated-chamber materials
{else}
M870 O ; open top vent for PLA/PETG/etc.
{endif}
{if chamber_temperature[initial_tool] > 35} ; if we need to heat the chamber
; min chamber temp section
M104 S{first_layer_temperature[initial_extruder]}
;M104 T{initial_tool} S{if idle_temperature[initial_tool] == 0}100{else}{idle_temperature[initial_tool]}{endif} ; set idle temp
G1 Z10 F720 ; set bed position
G1 X242 Y-9 F4800 ; set print head position
M191 S{chamber_temperature[initial_tool]} ; wait for minimal chamber temp
M141 S{chamber_temperature[initial_tool]} ; set nominal chamber temp
M107
M140 S[first_layer_bed_temperature] ; set bed temp

{else}
M141 S{if chamber_temperature[initial_tool] == 0}20{else}{chamber_temperature[initial_tool]}{endif} ; set nominal chamber temp
{endif}

{if first_layer_bed_temperature[initial_tool]<=60}M106 S70{endif}
G0 Z40 F10000
;M104 T{initial_tool} S{if idle_temperature[initial_tool] == 0}100{else}{idle_temperature[initial_tool]}{endif}
M190 R[first_layer_bed_temperature] ; wait for bed temp
M107

; --- RME Automatic Pressure-Advance Calibration ---
; Run after the bed wait and before the final print-area mesh. M976 homes and
; probes its own local anchor area before loading each requested MMU filament.
; It unloads before every probe and leaves the nozzle unloaded for the G29 MBL.
; CORE One has physical hotend 0; the second field is the logical MMU slot.
M976 A {if is_extruder_used[0]}0:0:{filament_type[0]}:{temperature[0]}{endif}{if is_extruder_used[1]}{if is_extruder_used[0]},{endif}0:1:{filament_type[1]}:{temperature[1]}{endif}{if is_extruder_used[2]}{if is_extruder_used[0] or is_extruder_used[1]},{endif}0:2:{filament_type[2]}:{temperature[2]}{endif}{if is_extruder_used[3]}{if is_extruder_used[0] or is_extruder_used[1] or is_extruder_used[2]},{endif}0:3:{filament_type[3]}:{temperature[3]}{endif}{if is_extruder_used[4]}{if is_extruder_used[0] or is_extruder_used[1] or is_extruder_used[2] or is_extruder_used[3]},{endif}0:4:{filament_type[4]}:{temperature[4]}{endif}

G29 G ; absorb heat

M109 R{((filament_notes[0]=~/.*HT_MBL10.*/) ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0] == "FLEX") ? 210 : 170)} ; wait for MBL temp

M302 S160 ; lower cold extrusion limit to 160C

{if filament_type[initial_tool]=="FLEX"}
G1 E-4 F2400 ; retraction
{else}
G1 E-2 F2400 ; retraction
{endif}

M84 E ; turn off E motor

; --- Mesh Bed Leveling (MBL) ---

G29 P9 X208 Y-2.5 W32 H4 ; limited MBL
M84 E ; turn off E motor

G29 P1 ; invalidate MBL and probe print area
G29 P1 X150 Y0 W100 H20 C ; probe near purge area
G29 P3.2 ; MBL interpolation
G29 P3.13 ; MBL extrapolation outside probe area
G29 A ; activate MBL

; --- Preparation for Purge Line ---

M104 S{first_layer_temperature[initial_extruder]}
G0 X249 Y-2.5 Z15 F4800 ; move away and ready for the purge
M109 S{first_layer_temperature[initial_extruder]}

G92 E0 ; reset extruder position
M569 S0 E ; set spreadcycle mode for extruder

T[initial_tool]
G1 E{parking_pos_retraction + extra_loading_move - 15} F1000 ; load to the nozzle

; Extrude purge line
G92 E0.0 ; reset extruder position
G1 X{first_layer_print_min[0] + 70} Y{first_layer_print_min[1] - 6} Z1 F6000
G1 X{first_layer_print_min[0] + 10} Y{first_layer_print_min[1] - 6} E70 F300
G1 X{first_layer_print_min[0] + 10} Y{first_layer_print_min[1] - 2} E4 F300
G1 X{first_layer_print_min[0] + 70} Y{first_layer_print_min[1] - 2} E70 F300
G92 E0.0 ; reset extruder position
G1 E-0.5 F2100 ; quick retract
G1 X{first_layer_print_min[0] + 73.5} F6000
G1 Z0.2 F6000
G1 X{first_layer_print_min[0] + 77} F6000.0 ; fast travel away from purge line
G1 Z2.0 F6000
G92 E0.0 ; reset extruder position

G92 E0
M221 S100 ; set flow to 100%

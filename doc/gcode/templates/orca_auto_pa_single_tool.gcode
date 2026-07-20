; RME AUTO PA FOR ORCASLICER: paste into Machine Start G-code immediately
; after the blocking bed-temperature wait, before full mesh/prime G-code.
; Do not place this in filament G-code. Uses normal-layer nozzle temperature.
M976 A 0:0:{filament_type[0]}:{nozzle_temperature[0]}

; RME AUTO PA: paste into Machine/Printer Start G-code immediately after the
; existing blocking bed-temperature wait (M190), before full mesh/prime G-code.
; Do not place this in filament G-code.
M976 A 0:0:{filament_type[0]}:{temperature[0]}

# Auto PA slicer setup

This guide configures PrusaSlicer or OrcaSlicer to run RME's RAM-only pressure
advance calibration before every file or serial print. Both slicers provide
`is_extruder_used[]` and `filament_type[]`, but their normal-layer nozzle
temperature placeholders differ: PrusaSlicer uses `temperature[]`, while
OrcaSlicer uses `nozzle_temperature[]`.

Copy templates as plain text and preserve every physical newline. Never join a
command to a preceding comment or slicer control directive; visual wrapping in
an editor does not create a G-code line. After slicing, verify that `M976 A ...`
appears on a standalone line in the exported G-code.

Only the printer/machine start G-code is changed. Do not add anything to a
filament preset's start or end G-code. Ready-to-paste files are provided for:

- [single-tool printers](gcode/templates/auto_pa_single_tool.gcode);
- [five-tool XL or INDX profiles](gcode/templates/auto_pa_xl_indx_5_tool.gcode);
- [eight-tool INDX profiles](gcode/templates/auto_pa_indx_8_tool.gcode);
- [five-slot MMU profiles](gcode/templates/auto_pa_mmu_5_slot.gcode).

Matching OrcaSlicer templates are provided for [single-tool](gcode/templates/orca_auto_pa_single_tool.gcode),
[five-tool XL/INDX](gcode/templates/orca_auto_pa_xl_indx_5_tool.gcode),
[eight-tool INDX](gcode/templates/orca_auto_pa_indx_8_tool.gcode), and
[five-slot MMU](gcode/templates/orca_auto_pa_mmu_5_slot.gcode) profiles.

Copy the complete matching template into the existing machine start G-code at
the insertion point below. Its placeholders automatically read the materials,
temperatures, and tools used by each sliced job.

## Before editing start G-code

1. Install an RME build containing `M976` on a supported loadcell printer.
2. In the printer's Filament menu, assign the material actually loaded in each
   tool or MMU slot. Batch validation compares this stored material name with
   the slicer's `filament_type[]` value. The spelling must match exactly (for
   example, `PLA` is not `PLA+`). `M865 Q` reports the stored assignments.
3. Save a copy of the original printer preset. In PrusaSlicer, enable Expert
   mode and open **Printer Settings -> Custom G-code -> Start G-code**. In
   OrcaSlicer, edit the printer preset and open **Machine G-code -> Machine
   start G-code**.
4. Keep the printer's normal compatibility checks, bed heating, final bed
   mesh, priming, tool-change and purge commands. Insert auto PA at the point
   described below; do not replace the complete factory start sequence.

## Where the command belongs

Heat and wait for the bed first so the small anchor mesh is measured at print
temperature. Put `M976` after the blocking bed wait, but before the normal full
bed mesh and before any prime line or wipe tower:

```gcode
M190 S{first_layer_bed_temperature[initial_extruder]}
; RME auto PA block goes here
; retain the preset's normal G28/G29/mesh and prime sequence below
```

Do not add a separate nozzle `M109` for calibration. Each batch entry supplies
its own temperature and `M976` sets and waits for the correct physical hotend.

## Single-tool printer

Use the batch form even for one tool so firmware checks the expected material:

```gcode
M976 A 0:0:{filament_type[0]}:{temperature[0]}
```

The OrcaSlicer equivalent uses `{nozzle_temperature[0]}`. Calibration uses the
normal-layer temperature because it represents most of the print; the
first-layer temperature may differ temporarily.

The four fields are physical tool, logical filament, expected material, and
temperature. For a normal CORE One, CORE One L, MK4/S, or iX, both indices are
zero.

## XL, INDX, and other multi-tool profiles

The following one-line manifest includes only tools that the sliced job uses.
It supports indices 0 through 7. Keep the
`M976 A ...` expression on one physical G-code line.

Use the ready-to-paste [five-tool XL/INDX template](gcode/templates/auto_pa_xl_indx_5_tool.gcode)
or [eight-tool INDX template](gcode/templates/auto_pa_indx_8_tool.gcode).

Batch mode validates the whole expanded manifest before doing anything. It
then uses the normal firmware toolchanger path to pick each requested physical
tool. Each logical index receives an independent PA result, pressure reference
and material flow ceiling. The last listed tool remains selected.

If the slicer profile exposes fewer tools, referencing a missing array element
may fail during slicing. Remove the unused trailing clauses from the printer
preset. For a four-tool INDX, use the eight-tool template but remove the clauses
for indices 4 through 7 so the command ends after tool index 3. A five-tool XL
normally ends at index 4.

For an eight-tool INDX profile, an expanded all-tool example is:

```gcode
M976 A 0:0:PLA:215,1:1:PLA:215,2:2:PETG:240,3:3:PETG:240,4:4:ASA:260,5:5:ASA:260,6:6:PC:275,7:7:PC:275
```

Use only the tools referenced by the sliced job and keep every material and
temperature synchronized with that tool's slicer and loaded-filament metadata.

## MMU profile

An MMU has one physical hotend (`0`) and several logical filament slots. Use
the same conditional template, but keep the first field at zero:

Use the ready-to-paste [five-slot MMU template](gcode/templates/auto_pa_mmu_5_slot.gcode).

Firmware heats the physical hotend and uses the MMU unload/load path between
entries. The calibration-specific load primes only 2 mm beyond the modeled
nozzle path instead of running the normal tool-change purge, preventing a long
loose filament loop over the bed. The PA excitation supplies the remaining
controlled extrusion. Firmware unloads before each local probe and unloads the
final requested MMU filament so the following full MBL runs with an empty
nozzle. The normal post-MBL initial-tool command loads the print filament.

## Verify before printing

Slice a small test and inspect the exported text G-code. Search for `M976 A`.
The slicer must have expanded it to a single line such as:

```gcode
M976 A 0:0:PLA:215,1:1:PETG:240
```

Do not print if braces, placeholder names, a leading/trailing comma, or an
empty manifest remain. Confirm that:

- every entry corresponds to a tool or MMU filament actually used by the job;
- temperatures match the intended first-layer filament profiles;
- material names exactly match `M865 Q` and the printer's Filament menu;
- the normal full mesh and prime sequence still follows the calibration;
- the generated first-layer objects do not occupy the documented front-edge
  anchor service area on machines without a purge bin.

On a deliberate material mismatch, firmware reports `M976 invalid batch or
loaded-material mismatch` before heating, moving, loading, or extruding. A
tool/MMU operation or calibration failure reports a separate batch failure and
stops the sequence.

## Fallback and repeated prints

The filament profile's `M572` pressure-advance value remains the fallback. If
the profile supplies no usable value, firmware uses its conservative material
preset. Results are never persisted and are reset at every normal or serial
print start. Repeating a logical slot within the same job reapplies its cached
result without another calibration extrusion.

For the command's complete behavior and diagnostics, see [M976](gcode/M976.md).

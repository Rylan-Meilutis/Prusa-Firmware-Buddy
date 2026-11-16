# INDX Orca tool-change optimization

This is an **opt-in slicer change**, not a claim that a firmware update alone
changes commands already baked into a print. Save a copy of the printer preset,
then replace **Machine G-code > Change filament G-code** with
[the complete template](templates/orca_indx_fast_toolchange.gcode). Re-slice;
do not edit a running print or silently rewrite existing G-code.

The first setting is `rme_eject_on_exit = false` (Entry Only). Set it to `true`
for Entry + Exit. Both branches now have a native entry ejection; the optional
exit ejection uses `F1` so an explicit slicer choice is not overridden by the
firmware's Entry Only filter. Old firmware ignores the extra `F1`, so the slicer
choice also works without the new firmware filter. Standalone maintenance and
calibration cleaning are not changed.

## What was actually found

The user-supplied template matches the embedded `change_filament_gcode` in the
repository's dual-colour Benchy G-code (printer preset `Cygnas X1 Indx`, Orca
2.5.0-dev). It has **no `G4` timed dwell** in either tool-change branch. The
no-tower branch has five `M400` drains, one thermal `M109`, two native ejections,
and two quick wipes. The tower branch has two `M400` drains, one `M109`, six
inline cleaner shakes, one exit eject, and one quick wipe. Those are not all
fixed waits, and removing them indiscriminately would be unsafe.

Local Orca `Cygnas X1` JSON presets found during inspection were older MMU
profiles with an empty change-filament field, not the INDX preset used by the
provided file. They were not modified. This repository template is the
authoritative optimized variant; installing firmware does not install it into
Orca automatically.

## Changes and preserved boundaries

- Remove the intermediate `P0 S1 L0 D0`: native `T...` already parks the old tool
  and picks the new one, with normal lock/unlock checks and motion safeguards.
  Keep `G27`'s retract, Z-hop and safe approach. This avoids P0's additional
  dock-to-printable-space escape followed by travelling back for pickup.
- Entry Only omits the newly purged pellet's exit eject. The preceding wipe,
  retract and native safe exit remain. The pellet is cleared on the next entry.
- Both branches use the same native entry wipe/eject. The tower branch's six
  handwritten `G750` shakes are replaced rather than accumulated on top of the
  native ejection.
- Remove caller `M400` after `G12 S1`: INDX `load_and_execute()` already ends
  with `planner.synchronize()`. The extra command adds no safety or fixed delay.
- Remove the drain immediately before the final blocking wipe: planner ordering
  finishes the queued purge/retract before wiping, and G12 drains before return.
- Keep the no-tower drain after fast reinsertion and before its acceleration
  change. Move the purge-completion drain **before** restoring `M201`/`M906`.
  `M906 P0` changes physical current immediately, without synchronizing; the old
  profile incorrectly requested it before its `M400`.
- Keep `M109` before any extrusion. `M104` starts heating first, allowing entry
  cleaning to overlap warm-up. Do not replace thermal readiness with `M104`,
  remove the required INDX residency, or attempt to preheat a passive parked
  tool as if it had an independent heater.
- No purge volume, purge feed formula, maximum speed, acceleration ceiling,
  retraction amount, lock mechanism dwell or nozzle-presence check is reduced.
  The tower branch no longer sends `M104 S0` just before a native park; native
  `park_procedure()` already stops heating before unlocking.

## Timing: known quantities versus measurements still needed

For the supplied tool7 -> tool0 Benchy transition, nominal commanded extrusion
times, ignoring acceleration and thermal delays, are:

| Component | Commands | Nominal lower bound |
|---|---|---:|
| Fast reinsertion | `E8 F1200` | 0.400 s |
| Pressure prime | `E3.4 F131.028` | 1.557 s |
| Purge first part | `E2.49578 F174.704` | 0.857 s |
| Purge Y outward | `Y-1.5 E1.87183 F174.704` | 0.515 s |
| Purge Y return | `Y1.5 E1.87183 F250` | 0.360 s |
| Final retract | `E-1.05 F2700` | 0.023 s |
| Total above | Same in both policy modes | 3.713 s |

For mixed XYZ/E moves, feed is referenced to XYZ path length, not E length.
Actual durations increase with acceleration and machine/filament limits.

The native exit ejection has about 144.5 mm of Y travel starting from the end
of the quick wipe (plus roughly 2 mm of X alignment), with repeated direction
reversals. Its lower-bound distance/speed is not its elapsed time: acceleration,
corner stops and firmware limits matter. The retained quick wipes and cleaner
entry/exit add motion too. Dock-to-dock XY travel depends on selected tools.
Native park/pick retain their 150 ms mechanical settling dwells, about 0.300 s
combined in a normal successful park/pick; these do not explain the reported
20-30 second tool-change duration by themselves. Presence verification has a 5 s maximum timeout per
check, not a mandatory 5 s sleep. INDX M109 has a 1 s residency qualification
plus material-dependent warm-up; qualifying time can overlap entry cleaning.

Therefore 20-30 s cannot be attributed to an unseen fixed G4. Thermal warm-up,
purge work, docking, retries and host delivery must be timed separately before
claiming 8-10 s. New no-tower output has two justified M400 commands rather than
five; Entry Only has one eject rather than two. Tower output has no redundant
M400 and one entry eject rather than six ad-hoc shakes plus an exit eject.

## Qualification

Compare one small two-tool print in Entry + Exit and Entry Only, recording the
time from `G27` through `G12 S91`, temperature at entry and duration of M109.
Inspect pellet retention, first extrusion quality, nozzle cleanliness, and bin
clearance for each material. Stop if retained waste curls onto the nozzle or
interferes with the cleaner. Use Entry + Exit for unqualified/custom cleaners.
Do not bypass a failed dock, collision, thermal or nozzle-presence safeguard to
meet the target. Final standalone maintenance ejects remain available.

## Verification performed

The installed OrcaSlicer 2.5.1 CLI successfully sliced generated two-object,
two-tool test projects in all four combinations: tower off/on and entry-only /
entry-plus-exit. Tests used temporary settings/cache/output directories, not
the user's live presets. The generated no-tower blocks contain one or two
ejections according to the choice, exactly two M400 drains, and M109 before
extrusion. Tower blocks contain the same explicit choice without the old six
inline shakes. Orca itself also emits zero-duration `G4 S0` synchronization
commands around wipe-tower work; these are not positive timed sleeps and were
not stripped from generated output.

Unit tests compare the optimized template with the checked-in supplied
original, preserving every purge/retraction expression, G27/T motion setting,
M109 temperature, acceleration setting and Z clearance. These tests and the
real slicer parse do not replace mechanical/thermal print qualification.

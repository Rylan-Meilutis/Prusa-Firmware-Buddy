# RME 6.9.0 port audit

## Idle chamber heating, manual motion and material labels — 2026-09-16

Ported the 6.10.1 idle bed-assist policy, model-specific G123 manual movement
guards, homing-aware Move Axis UI and material/color labels. Internal spool
identifiers remain intact. Normal G0/G1/G2/G3 print paths are unchanged.
Explicit bed commands, safety shutdown and print start revoke idle bed boost.
Full release build validation is pending. Hardware thermal response, safety
timeout, print transitions, manual jogs and material-label checks remain pending.

## Independent lighting controls — 2026-09-16

Chamber On/Locked honor Active-profile channel masks. LCD Off/On uses a
separate runtime flag and wake window, not the chamber selector. TUNE publishes
LCD state and supported current-print brightness values in a bounded snapshot.
INDX build and 54 RME tests pass. Hardware door-wake, timeout and disabled-channel
checks remain required before release.

## Settings navigation restoration

Verify Settings > Lights exposes Deep Idle, Idle, Active and Printing profiles
and supported chamber/status/external controls. Verify Serial Printing, Printer
Lock, Heater Safety, PID, USB export, custom filament metadata and User Interface
> UI Theme. Full matrix linking and physical menu checks are required.

This audit records the version-specific safety work for the RME port from
6.8.1 to upstream 6.9.0.

## High-risk upstream overlaps

### 2026-09-16 lighting fix changelog

Restart the LCD/idle status-LED countdown on chamber-mode changes and timed
On expiry; release stale legacy host holds. Preserve print, door, operation and
safety priorities. Matching 6.10.1 host gate passed 52 regression tests /
432,320 assertions. Require the full 6.9.0 matrix and uploaded digest check;
physical idle-cycle testing remains pending.
Follow-up: door/local activity now clears slider Off and restores automatic
chamber lighting alongside the LCD. Updated 6.9 host gate: 53 cases / 432,334
assertions passed. Rebuild both full matrices before publishing this follow-up.

- Persistent store definitions and journal hashes
- Serial-print finalization and cleanup
- INDX waste-bin park, cooldown, reheat, and return motion
- Side-strip and INDX head-light state
- Filament menus, stuck-filament responses, and Connect telemetry
- Shared transfer pause/resume behavior
- Generated translations and constrained MINI resources

## Port decisions

- Upstream menu organization and generated translations are authoritative.
- RME's bounded out-of-band parser and serial lifecycle remain authoritative
  where emitting file-print completion markers would reset a live host's line
  numbering.
- Upstream waste-bin thermal and safe-return behavior is combined with RME
  pause/resume notifications; neither behavior is discarded.
- Material family and selected profile remain independent fields.
- The former `Custom Filament Color RGB` journal key cannot coexist with
  upstream's `Tool Offset Sensor Displacement` because both map to journal ID
  5777. The RME array uses `RME Custom Filament Color RGB` on 6.9.0. The old
  value is deliberately not migrated because its numeric identity is
  ambiguous in the combined schema.

## Required release gates

1. Build and run `rme_protocol_tests`, `transfers_tests`, `connect_tests`, and
   persistent-store tests.
2. Verify Connect INFO fixtures contain separate `material` and `profile`.
3. Run the journal hash generator through at least one real firmware preset.
4. Build the most constrained translated MINI preset and CORE One INDX before
   the full matrix.
5. Run `./build.py --final --versions 6.9.0 --jobs 15` and require every
   advertised preset to link within every memory region.
6. Publish only artifacts produced by that successful final matrix and verify
   their GitHub SHA-256 digests.
7. Run the exhaustive INDX workflow/phase mapping tests and verify a CORE One
   INDX build reports tool detection, guarded dock selection, pickup/park
   failures, and all calibration FSMs through `DIALOG QUERY`.
8. Verify a dock selection cannot be submitted outside its active FSM phase,
   and parameterless `M104 S0`, `M107`, and `M105` remain valid with all tools
   parked.

Passing host tests and linker limits substantially reduce regression risk but
do not replace an on-printer smoke test of boot, RME session open, upload
resume, serial print, and lighting hold.

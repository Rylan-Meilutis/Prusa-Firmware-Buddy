# RME 6.9.0 port audit

## September 19 touchscreen load loop and flexible Auto PA

Ported the color-to-brand screen replacement and single-close completion,
plus flexible PA feeds of 0.2/1.5 mm/s and 2 mm/s cleanup retraction. Cache
format 2 invalidates old measurements and preserves flexible classification
for matching runtime pressure-reference speeds. Rigid calibration rates and
five-cycle cooling/ejection/main-wiper behavior are unchanged.

Final 6.9 matrix from a7b42b0ed passed 15/15 using the shared dependency
environment, resolving the earlier worktree bootstrap issue. Paired 6.10.1
matrix from 3ddf5fec3 passed 15/15. Shared regression suites: 61 RME cases
/ 432440 assertions, 19 extrusion cases / 64 assertions, 2 touchscreen
navigation source checks. Physical validation remains pending.

## September 18 release validation

Final complete matrix from 5ee44226c: 15/15 presets passed in 5m26s.
INDX flash 67.01%, aggregate RAM 77.47%, CCMRAM 97.16%; MINI flash peaks
at 99.95%. Shared policy and regression source matches 6.10.1; suites pass
60 RME cases / 432426 assertions and 17 extrusion cases / 59 assertions.
Plugin 0.1.0b107 is published targeting beta with 222 Python tests and the
lighting/retry-notice DOM test passing. Hardware gates below remain pending.

## Serial results and confirmed host retry — 2026-09-18

Ported a9cfbafbc from 6.10.1. Both Finished and Aborted retain the serial
result FSM, with filtration Settings, Continue and bed-clear-confirmed Reprint.
Reprint queues the host action through the Marlin thread; the matching plugin
validates file identity, connection, lock and job state before a full restart.
No streamed-file replay or new persistent allocation is added to firmware.
Typed temperature-wait messages are filtered from serial notifications and
Messages history, without suppressing thermal errors or temperature telemetry.
Upstream automated validation: 60 RME cases / 432426 assertions, INDX build,
222 plugin Python tests and lighting/retry-notice DOM checks passed. Physical
success/cancel/error results, filtration controls and confirmed restart remain
hardware gates; a new print must never resurrect an older result screen.

## Persistent PA cache, dock fan and serial lighting — 2026-09-18

Ported 1363f8565 from 6.10.1. Fixed-size CRC-protected PA records use
atomic internal-flash file replacement without changing EEPROM journal size.
Only accepted measurements persist; unload and relevant factory reset clear
records. Exact metadata/nozzle/temperature matching skips cached automatic
calibration, and tool selection restores the corresponding PA/flow/reference.
Manual F1 refresh remains available for same-metadata filament replacement.
Uncached calibration holds the dock fan at full speed and restores it on exit.
Every five-cycle/final pellet ejection retains 15-second cooling and adds two
native main-wiper cleaning sequences. No keep-out exceptions are introduced.
Serial print traffic no longer releases temporary lighting Off; idle activity,
local input and configured door holds retain wake behavior.
Upstream validation: 59 RME cases / 432412 assertions and 17 extrusion cases /
59 assertions. Version-specific full build and hardware validation pending.

## Door-open RME lighting priority — 2026-09-17

Ported the 6.10.1 level-triggered door hold. Temporary chamber Off/On cannot
override an already-open door when Door Holds Active is enabled. Apply the
policy on host selection and before timer expiry; keep configured active
channel masks/brightness, Locked mode and independent LCD controls intact.
Chamber toggles do not restart the idle countdown while the door holds active;
the existing closing edge starts the normal timeout. No sensor calibration
changes or heap allocations. Shared regression suite: 58 cases / 432,403
assertions. INDX build passed from 717f7bedc (66.90% flash, 77.47% RAM);
physical door/toggle testing remains pending. Other machine images unchanged.

## INDX loadcell, cancellation filtration and PA cleanup — 2026-09-17

Ported the 6.10.1 step-gap tolerance and velocity averaging for loadcell
pressure monitoring, plus both detection switches in Settings and Tune.
M591 enable/disable policies and valid PA reference requirements remain.
Filtration now exits active-job mode at Aborted, not only after leaving the
result screen; finishing cleanup retains eligibility. Manual cycle controls
use the same predicate. PA cleanup stays at five cycles with 15-second
fan-assisted cooling for every material and a main-wiper pass before ejection.
Shared host regressions pass: 14 extrusion cases / 32 assertions and 56 RME
cases / 432,364 assertions. INDX release build passed from 319ef9f2a
(66.90% flash, 77.47% RAM);
hardware break recovery, filtration and pellet release remain unverified.
Only the INDX image is updated; other machine images remain unchanged.

## Idle chamber heating, manual motion and material labels — 2026-09-16

Ported the 6.10.1 idle bed-assist policy, model-specific G123 manual movement
guards, homing-aware Move Axis UI and material/color labels. Internal spool
identifiers remain intact. Normal G0/G1/G2/G3 print paths are unchanged.
Explicit bed commands, safety shutdown and print start revoke idle bed boost.
Full 15-image release matrix passed (9m17s), built from cfbac674e. Shared
RME policy tests: 55 cases / 432,352 assertions; chamber: six / 35 assertions.
INDX flash 66.88%, RAM 77.47%; MINI language flash 99.92%.
Hardware thermal response, safety
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

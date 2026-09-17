# RME 6.10.1 port audit

The upstream 6.10.1 delta is limited to translations, welcome-screen logic,
Prusa error-code data, and the version number. It has no direct source overlap
with the RME parser, transfer, firmware-update, filament, lighting, or serial
session implementations.

## Release gates

### Open-door lighting priority — 2026-09-17

set_door_open exits early for unchanged sensor levels, so an RME Off/On set
after opening could survive until another door edge. Apply door-hold priority
both in set_chamber_mode and before temporary-mode expiry in update. Off/On
return to automatic configured active lighting while Locked stays Locked.
The shared countdown helper releases legacy holds but does not rewrite the
timestamp during an enabled open-door hold. The existing closing edge starts
the timer. No door-sensor calibration or LCD override behavior is modified.
Regression coverage includes repeated Off/On with a stable open sensor,
expiry suppression, close/restart, Locked and disabled door hold.
All 58 RME cases / 432,403 assertions pass. INDX final build from 1a4533e79 passes
(66.90% flash, 77.45% RAM). Publication covers INDX on 6.10.1 and 6.9.0;
physical testing remains pending.

### INDX loadcell, cancellation filtration and PA cleanup — 2026-09-17

Settings and Tune previously excluded INDX from the runout item and omitted
the existing movement item entirely. Both now expose the two existing INDX
switches, retaining M591 S/U with P persistence and queue-failure rollback.
Non-INDX menu contents are unchanged. Check both menus on hardware; explicit
slicer M591 overrides can still disable a detector after enabling it here.

The sample-level pressure monitor previously discarded all forward/fault
evidence whenever executed E was unchanged between loadcell samples. Added
bounded 150 ms step-gap tolerance and velocity averaging over those gaps;
no motion evidence is accrued during the gap itself, and real travel or
negative E still resets evidence. Quantized healthy extrusion, subsequent
pressure collapse, missing pressure, and travel/retraction regressions pass.
This does not enable disabled M591 policies or create a reference when PA
calibration has not supplied one. Confirm the failing job's M591 settings.

Filtration used is_abort_state(), which includes the terminal Aborted screen.
The shared filtration job predicate now excludes Aborted and includes all
finishing cleanup states; the manual cycle menu uses the same predicate.
Configured material/temperature eligibility and post-print enable remain in
effect. State regression covers Printing, Paused, abort cleanup, finish
cleanup, Aborted, Finished, Exit and Idle.

PA retains five-cycle batching, increases all material cooling to 15 seconds,
then uses quick_clean after the dedicated strand-break wipe and cooling,
before eject_blob. Native cleaner offsets and keep-outs remain authoritative.
Host suites: 14 extrusion cases / 32 assertions; 56 RME cases / 432,364
assertions. INDX release build with both restored menu switches passes from 73c32dc4c
(66.89% flash, 77.45% RAM); no new heap
allocations. Publish scope is INDX on 6.10.1 and a matching 6.9.0 port;
other machine images remain unchanged. Hardware checks remain pending.

### Manual movement and material UI — 2026-09-16

G123 now enforces per-model manual ranges and requires known positions;
Move Axis exposes Auto Home and disables unknown axes. Dedicated service
workflows remain separate from manual movement. Tool picker and Loaded
Filaments use the authoritative material family plus color swatches, keeping
profile IDs intact in storage. RME regression gate: 55 cases / 432,352
assertions; chamber policy: six cases / 35 assertions. CORE One, INDX,
CORE One L, MINI and XL final validation builds passed, including the cleaner
exit guard and post-homing display refresh. The complete 15-image final release
matrix subsequently passed (9m22s), firmware source edf912b4d. INDX flash
66.87%, RAM 77.45%; MINI language flash 99.93%. Hardware checks remain pending.

### Idle chamber bed assistance — 2026-09-16

RME Compatibility b99 was published first with passive INDX temperature
presentation and active-tool/bed/chamber controls. Firmware now owns a temporary
idle bed boost for a chamber heating request, bounded by 100 C and the hardware
bed limit. Print start, explicit bed commands and heater shutdown revoke that
ownership; automatic thermostat updates do not refresh safety timeouts.
The chamber policy suite passes six cases / 35 assertions. Physical thermal
response, sensor failure, heater timeout and print-transition validation remain
required; see the release playbook. No dynamic allocation or extra host polling
was added.

### Independent lighting controls — 2026-09-16

Ported the Active-profile internal/external channel-mask fix and independent
LCD control from 6.9.0. TUNE streams LCD state and per-print brightness in
one bounded snapshot; unsupported channels remain -1. The 54 RME host tests
pass (432,343 assertions). Physical door wake, LCD timeout and profile-mask
validation remains pending; see the release playbook checklist.

### Settings navigation restoration

Restore existing RME settings screens lost during upstream menu refactoring.
Verify Settings > Lights state profiles, chamber mode/timers, status colors
and external bar where supported; Settings > User Interface > UI Theme;
and Serial Printing, Printer Lock, Heater Safety, PID, USB settings export
and custom filament metadata entries. Validate all model links and memory
budgets with the full matrix; physical navigation testing remains pending.

### 2026-09-16 lighting fix changelog

Restart the LCD/idle status-LED countdown on chamber-mode changes and timed
On expiry; release stale legacy host holds. Preserve print, door, operation and
safety priorities. 52 regression tests / 432,320 assertions passed, and the
changed INDX lighting translation unit compiled. Full release matrix and
uploaded digest verification are required below; physical testing is pending.
Follow-up: door/local activity now clears slider Off and restores automatic
chamber lighting alongside the LCD. Updated shared host gate: 53 cases /
432,334 assertions passed. Rebuild both matrices before publishing this change.

1. Run `rme_protocol_tests`, `transfers_tests`, `connect_tests`, and
   `eeprom_unit_tests` from a 6.10.1-configured host-test build.
2. Build the complete final 6.10.1 matrix and require every advertised preset
   to remain within every linker-enforced memory region.
3. Publish only artifacts produced by that successful matrix.
4. Compare every GitHub asset digest with its local SHA-256 value.
5. Run the exhaustive RME INDX workflow mapping tests and a CORE One INDX
   link. Confirm `DIALOG QUERY` reports tool detection, slot selection,
   pickup/park errors, and each calibration FSM without allocating memory.
6. Verify the guarded slot-selection response cannot be applied outside the
   active unknown-tool selection phase, and verify parked parameterless
   temperature/fan commands remain valid.

Host tests and linker limits substantially reduce regression risk but do not
replace an on-printer smoke test of boot, RME session open, upload/resume,
serial printing, firmware status, and lighting hold.

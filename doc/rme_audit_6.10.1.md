# RME 6.10.1 port audit

The upstream 6.10.1 delta is limited to translations, welcome-screen logic,
Prusa error-code data, and the version number. It has no direct source overlap
with the RME parser, transfer, firmware-update, filament, lighting, or serial
session implementations.

## Release gates

### September 19 touchscreen load loop and flexible Auto PA

Replace the color picker with the brand picker rather than stacking it;
complete brand selection with one deferred close. Source navigation checks
pass (2 tests); touchscreen validation is pending.

Flexible Auto PA feeds are 0.2/1.5 mm/s with 2 mm/s cleanup retraction.
Cache format 2 and job results retain the flexible classification so pressure
monitor references use the measured velocities. Rigid rates, confidence gates,
five-cycle ejection, cooling and main-wiper passes are unchanged. No new heap
allocation is introduced. Older cache entries recalibrate once.

Validation: 61 RME cases / 432440 assertions, 19 extrusion cases / 64 assertions,
and INDX build passed (67.03% flash, 77.47% aggregate RAM, 97.16% CCMRAM).
Final dual matrices passed (15/15 each), from 3ddf5fec3 for 6.10.1 and
a7b42b0ed for 6.9.0. Physical FLEX feeding and touchscreen checks remain pending.

### September 18 release validation

Final matrix from 1ecd7e2b2: 15/15 presets passed in 5m24s. Paired 6.9.0
matrix from 5ee44226c: 15/15 passed in 5m26s. INDX flash 67.01%, aggregate
RAM 77.45% (6.10.1) / 77.47% (6.9.0), CCMRAM 97.16%. MINI flash peaks
at 99.96% / 99.95%. Shared suites pass 60 RME cases / 432426 assertions
and 17 extrusion cases / 59 assertions. RME Compatibility b107 is published
on beta; 222 Python tests and lighting/retry-notice DOM tests pass. Physical
result-screen, host retry, lighting and cache/wiper checks remain pending.

### Serial result screens and host retry — September 18 update

Retain the serial printing FSM for both Finished and Aborted. Share result
pagination, duration/end-time and filtration countdown handling in both UI
modes. Settings opens the existing chamber filtration screen; Continue exits
the result screen. Reprint defaults to No in the bed-clear confirmation and
queues `M118 A1 action:rme_retry` through the Marlin thread. The RME plugin
validates the same selected completed local job and uses OctoPrint start_print,
not resume. Host checks cover connection generation, lock, transfer, active-job
states, duplicate requests and file identity. No firmware file replay/cache or
new persistent allocation is introduced. Requires matching plugin changes.

Hardware gate: verify success, manual cancel and bounds-error cancel retain
their respective result heading, filtration settings and countdown; dismiss
with Continue and restart from the printer only after clearing the bed. Verify
new jobs and filament/filtration dialogs do not resurrect an older result.

Automated validation: COREONE INDX build passed (RAM unchanged at 77.45%);
60 firmware protocol tests / 432426 assertions passed. Matching plugin merged
with beta b106; 222 Python tests and lighting/retry-notice DOM tests passed.
Included in the September 18 persistent release update with plugin b107.

### Serial temperature-wait messages — September 18 update

Filter typed temperature-wait records at outbound serial status reporting and
serial Messages history consumption. This removes both waiting labels and the
formatter's current/target temperature lines without matching localized text
or suppressing thermal errors. Current operation progress and raw temperature
telemetry remain intact. History IDs advance even for filtered records.

### Serial-print RME lighting override — September 18 update

SerialPrinting previously called activity_ping for every streamed G0/G1,
which released temporary chamber Off and screen Off. Gate serial wakeups on
serial_print_active: idle commands retain wake behavior, print-stream traffic
does not. Local input and door holds are unchanged. Regression covers repeated
stream activity preserving Off, idle wakeup and door wakeup. Hardware check:
toggle chamber/LCD Off during an OctoPrint job with the door closed, then open
the door and verify the configured wake behavior.

Validation: INDX build passes; RME unit suite passes 59 cases / 432412
assertions. Companion plugin navbar/slider controls no longer expire after
15 seconds of delayed telemetry; stale values are identified as last reported.
Plugin lighting DOM regression and nine print-control backend tests pass.

### Persistent INDX PA cache — September 18 update

INDX PA cooling/cleaning follow-up: scoped dock-fan full-speed control covers
uncached batches and standalone calibrations, including heating and early
returns. All-cache hits do not change the dock fan. Every periodic/final
pellet ejection uses two native `Sequence::clean` main-wiper sequences after
ejection; five-cycle grouping and 15-second cooling remain unchanged. Native
G750 calibration and motion checks are retained. Hardware validation must
confirm dock airflow and actual wiper contact, including custom cleaner files.

Versioned `/internal/pa-cache-v1-<slot>.bin` files store fixed-size records per
loaded virtual slot with CRC32 and atomic temporary-file replacement. The
EEPROM journal lacks capacity for eight reference records, so its size and
headroom gates remain unchanged. Factory reset clears these files when
calibrations, hardware, printer state or user profiles are reset.
Automatic M976 validates the full manifest first, then
restores cache hits before any tool-change, heating or purge work. All-hit
batches return before opening the calibration FSM. Partial batches visit
only misses. Manual batches propagate F1 to nested commands, invalidating
old entries before measurement. Only accepted measurements are persisted;
the low-confidence fallback path remains job-only. Unload clears the record.
Exact keys include metadata, physical tool, nozzle, temperature and acceptance
settings. Same-metadata spool/nozzle replacement requires manual refresh.
Tool selection reapplies the slot's PA/flow and loadcell reference; merely
loading the last cache entry cannot make its PA authoritative for all tools.
Hardware checks still required: reboot/all-hit no-motion startup, one changed
slot in a mixed batch, unload/reload, F1, and print-time tool changes.

Validation: CORE One INDX build passes (67.00% FLASH, 77.45% RAM).
Extrusion calibration tests pass: 17 cases / 59 assertions, including flash
roundtrip, corrupt/truncated records, key invalidation and per-tool selection.
RME protocol tests pass: 59 cases / 432412 assertions. For a forced batch,
place F1 before the manifest, for example `M976 F1 A 0:0:PLA:220`.

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

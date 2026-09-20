# 6.10.1-RME Firmware

## Release update — restore configured print lighting, 2026-09-20

Validation: all 15 release variants passed from `b929ada1f`.

- Chamber On during printing now restores the configured print-state brightness
  and channel enable mask. Disabled channels and zero-brightness channels stay
  dark instead of being forced on at full brightness. Off remains off.
- Shared RME regression suite: 64 cases / 432542 assertions passed, including
  repeated Off/On transitions with disabled and zero-brightness channels.
- Compatible with RME plugin b109; no plugin update is required for this fix.
- No further TPU changes in this update. A tool swap resolved the reported
  loading failure; reliable TPU Auto PA still needs hardware validation.

## Release update — flexible Auto PA and recovery, 2026-09-20

Validation: 15/15 release variants passed from `62145066b`. Host suites:
21 calibration, 10 stall-detector, 63 RME protocol, and 12 EEPROM cases passed.
Plugin b109: 232 Python tests (4 skipped), six UI suites plus Auto PA selector
DOM synchronization passed. MINI excludes the unsupported Auto PA store item
to fit flash. Hardware validation is still pending.

- TPU/TPE/FLEX excitation now uses averaged E-step velocity at 0.2/1.0 mm/s,
  retaining confidence/SNR rejection. Flexible calibration flow is capped at
  2.4 mm3/s; INDX native flexible feeds and unload ramming are capped at 2 mm/s.
- Rejected PA fits no longer arm calibrated pressure fault detection. Recovery
  discards stall flags collected during blocked unload/load operations.
- INDX flexible mesh probing wipes, cools to at most 120 C, wipes again, and
  restores the prior target afterward. Eight unsuccessful contacts return to
  normal recovery; contact validity checks are not weakened.
- Cache format v3 deliberately recalibrates once after updating. Successful
  fits persist and are verified on read-back; fallback results are not cached.
- Companion plugin: 0.1.0b109. Clear any existing gear wrap before testing.
  These are software-tested mitigations, not hardware-validated 95A TPU settings.

- Persist Off / Auto / On calibration policy, shared between the printer and
  RME control tab. Auto retains temperature-sensitive flash cache reuse; On
  forces measurement and Off skips automatic commands without heating/motion.
- Report cache-save success and cache-miss categories for troubleshooting.
- Preserve scoped dock-fan restoration across INDX batch and individual
  calibration exits, including cancellation and failures.

## Release update — print lighting and host progress, 2026-09-19

Final matrix: 15/15 passed from `1f2af6736`. Shared RME tests: 63 cases /
432492 assertions. Companion plugin: 0.1.0b108 (229 Python tests, six UI suites).

- Add capability-gated RME PROGRESS SET for authoritative OctoPrint completion
  and ETA during serial jobs. Preserve unknown/paused estimates, expire stale
  ownership after 60s, and avoid double speed scaling of host estimates.
  MINI retains its existing M73/M117 path due to language-build flash limits.

- Separate binary print On/Off from idle Off/On/Locked timers and door activity.
- Report effective print lighting even during transient UI state changes;
  include print ownership in RME_TUNE for local and streamed jobs.
- Use only Off/On in the touchscreen chamber selector during printing.
- Reapply current-print brightness changes immediately. Fixed-size state only;
  no new heap allocation. Physical validation remains pending.

## Release update — TPU/TPE material aliases, 2026-09-19

Final build from `bd9fc3499`: 15/15 presets passed in 8m44s.

- Auto PA accepts TPU, TPE and FLEX as the same flexible family, preserving
  custom profile names and rejecting rigid-material mismatches.
- Resolve effective flexible flags from the configured material on read so
  existing host-created profiles with base=FLEX and flexible=0 also receive
  native loading/purge slowdown and the automatic-retraction exclusion.
- Shared regression suites pass: 61 RME cases / 432475 assertions and
  19 extrusion cases / 64 assertions. Physical TPU/TPE validation is pending.

## Release update — INDX load metadata prompt loop, 2026-09-19

- Replace the color picker with the manufacturer picker during filament load.
  Repeated deferred Close calls previously left the color picker on the screen
  stack, reopening brand selection after both choices had already been made.
- Both existing-brand and Add Manufacturer completion now close once.
  Metadata selection and the preheat FSM response remain unchanged.

## Release update — gentler flexible-filament Auto PA, 2026-09-19

- Flexible calibration uses 0.2/1.5 mm/s filament feeds instead of 0.8/8 mm/s;
  cleanup retraction is reduced from 20 to 2 mm/s. Rigid rates are unchanged.
- Recognize the flexible flag and FLEX/TPU/TPE material identity, including
  mapped spool profiles. Pressure monitoring follows the calibration speeds.
- Cache format 2 invalidates older measurements once and includes flexible
  classification in the key. Existing confidence checks remain unchanged.
- Five-cycle purge ejection, cooling, dock fan and main-wiper passes remain.
  Physical flexible-filament validation is still required.

Validation: all 15 final firmware presets passed from `3ddf5fec3`.
Both release matrices passed (30/30 images). Regression suites passed:
61 RME cases / 432440 assertions, 19 extrusion cases / 64 assertions,
and 2 touchscreen navigation source checks. No new heap allocations.

## Release update — serial results, lighting and INDX PA cache, 2026-09-18

This update rebuilds the complete supported machine matrix, superseding the
INDX-only updates below. Reprint requires the matching RME Compatibility plugin.
Physical printer validation remains pending.

Validation: all 15 final firmware presets passed from `1ecd7e2b2`.
Shared regression suites passed: 60 RME cases / 432,426 assertions and
17 extrusion-calibration cases / 59 assertions. INDX uses 67.01% flash and
77.45% aggregate RAM; the tightest MINI flash target is 99.96%.
Use RME Compatibility **0.1.0b107** (OctoPrint Beta channel) for host retry.

- Keep serial-print result screens visible after cancellation as well as
  success, including legacy serial UI mode. Settings opens filtration controls;
  Continue dismisses the result without resuming the aborted job. Reprint asks
  for bed-clear confirmation and requests a full restart through the matching
  RME Compatibility plugin, which owns the streamed file.

- Suppress routine hotend/bed/chamber temperature-wait notifications and their
  current/target temperature text from the serial Messages page. Normal heater
  progress, temperature telemetry and thermal errors are unchanged.

- Fix RME light Off flickering back on during serial prints: streamed job
  commands no longer count as user lighting activity. Idle commands, local
  interaction and the configured door-open hold retain their wake behavior.

- Uncached INDX Auto PA runs the dock fan at full speed during heating,
  calibration and cleanup, restoring its previous setting on exit.
- Every PA pellet ejection now runs two full native main-wiper cleaning
  sequences after strand separation, cooling and ejection. The five-cycle
  ejection interval and 15-second cooling dwell remain unchanged.

- Successful M976 measurements are saved per loaded slot in internal flash,
  including PA, flow limit and the loadcell pressure reference.
- Automatic batches skip cached slots before picking tools or heating/purging.
  An entirely cached batch applies the results without calibration motion.
- Cache matching includes profile/material, color, manufacturer, physical tool,
  nozzle diameter, calibration/profile temperature and confidence/SNR settings.
  Unloading clears the slot record. Unknown filament and low-confidence
  fallback results are not persisted. Cache hits do not write flash. Records
  use checksums and atomic file replacement without enlarging the EEPROM journal.
- Manual calibration or `M976 F1 A 0:0:PLA:220` refreshes the cache (adapt the
  manifest to the loaded tools; put `F1` before `A`). Use this after
  replacing a nozzle with the same diameter or changing filament properties
  without changing its metadata. A slot retains its latest successful result,
  not a history of every spool/temperature combination.
- Selected-tool PA and loadcell references are restored on INDX tool changes.
  File access is bounded to one small record at a time; there is no resident
  heap cache and no motion/keep-out exceptions are introduced.


## INDX test update — door-open RME lighting priority, 2026-09-17

Updated INDX image only; download `coreone_indx_6.10.1-RME.bbf` again.
Other machine images remain unchanged.

INDX build passed from 1a4533e79; 58 RME cases / 432,403 assertions pass.

- With Door Holds Active enabled, an already-open door now cancels temporary
  RME Off/On overrides continuously. It uses the configured active profile;
  it does not enable every LED channel or force full brightness.
- RME chamber toggles no longer restart the shared idle countdown during an
  open-door hold. Closing the door starts the usual activity timeout.
- Locked mode, disabled door-hold behavior and the separate LCD control are
  preserved. Physical door/toggle validation remains pending.

## INDX test update — flow monitoring and cleanup, 2026-09-17

This update replaces only `coreone_indx_6.10.1-RME.bbf`. Download it again
even if you already have 6.10.1-RME. Other machine images remain the September
16 build; the shared fixes described below are not yet rebuilt for them.

- Restore Loadcell Filament Runout and Loadcell Filament Movement switches
  in INDX Settings and in-print Tune. Both save through the existing M591
  permanent-setting handlers. Slicer-issued M591 overrides still apply.

- Preserve loadcell fault evidence across up to 150 ms between executed E
  steps. Quantized motor feedback must not reset the monitor on every sample;
  real travel/retraction still resets evidence. Detection still requires a
  valid PA pressure reference and enabled M591 runout/movement policies.
- Treat Aborted as a terminal job state for filtration, so a qualifying
  cancelled print starts the configured post-print cycle while its result
  screen remains open. Keep finishing/unloading/parking in active job mode.
  The manual filter-cycle control is also available on the Aborted screen.
- All INDX Auto PA materials now receive 15 seconds of fan-assisted pellet
  cooling (previously 4 or 12 seconds). Keep five high/low cycles per cleanup
  plus a final partial batch. After strand break-off and cooling, run the
  native calibrated main-wiper pass before pellet ejection.
- Host regressions: 14 extrusion cases / 32 assertions and 56 RME cases /
  432,364 assertions pass. INDX release build with restored menu switches
  passes from source 73c32dc4c (66.89% flash, 77.45% RAM);
  no new heap allocations. Physical filament-break recovery, cancelled-print
  filtration, and cooled-pellet/main-wiper behavior require hardware testing.
  Hardware validation is pending; this INDX update is provided for testing.

## Build validation — 2026-09-16 chamber/motion update

All 15 release images passed; firmware source edf912b4d. Shared regression
suites pass: 55 RME cases and six chamber-policy cases. Hardware heating,
manual-motion and display validation remains pending. Normal print-motion
handlers G0/G1/G2/G3 are unchanged by this update.

## Manual movement and loaded-material UI — 2026-09-16

- Enforce model-specific manual travel limits inside G123, not only serial
  G0/G1. INDX excludes cleaner/dock service areas; XL excludes rear docks.
- Move Axis offers Auto Home and disables unknown axes instead of presenting
  an invented home corner as a usable position. Backend homing checks also
  protect queued or manually supplied G123 commands.
- Tool selection and Loaded Filaments show the base material and loaded-color
  swatch, not profile identifiers such as PLA-00D. Stored profiles are unchanged.
- Physical jog/keep-out and display validation remains pending.

## Idle chamber bed assistance — 2026-09-16

- A chamber target set while idle can heat the bed automatically on CORE One,
  CORE One INDX and CORE One L. Temporary boost is chamber target +40 C,
  capped at 100 C and the machine's bed limit; existing higher bed targets stay.
- Restore the previous bed target at chamber temperature, target Off or print
  start. Explicit bed commands and safety shutdown take priority until the next
  chamber request. Automatic updates do not reset heater safety timeouts.
- Normal print bed targets and print cooling remain unchanged. Existing chamber
  cooling controls remain in effect. Hardware heating/cooling validation pending.

## Independent lighting controls — 2026-09-16

- Chamber On/Locked respect internal/external Active-profile enable settings.
- Independent LCD on/off protocol and live LCD/per-print brightness snapshots.
- Full 15-image release matrix and 54 RME unit tests passed; physical validation pending.

## Summary

6.10.1-RME carries the complete RME serial, transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack forward to upstream Prusa
Firmware Buddy 6.10.1.

## Integration changes

- Restore missing RME Settings links: Lights now exposes Deep Idle, Idle,
  Active and Printing profiles plus chamber mode/timers and supported status
  and external-light controls. Serial Printing, Printer Lock, Heater Safety,
  PID, USB settings export, custom filament colors/manufacturers and UI Theme
  are reachable again on supported displays. Hardware menu validation remains
  pending; unavailable hardware controls remain guarded.

- Restart the shared screen and idle status-LED countdown when the chamber
  light selector changes or timed On expires. Clear stale legacy RME active
  holds without overriding print, door-open, operation or safety behavior.
  Door transitions and local activity also clear slider Off so the chamber
  lights wake with the LCD. Locked remains on; polling is not activity.
  Regression suite: 53 tests, 432,334 assertions. Hardware idle-cycle validation
  remains outstanding.

- Add synchronized live print controls with RME Compatibility 0.1.0b95:
  Off/On/Locked chamber lighting, speed, per-slot flow, and stealth mode.
  Firmware Tune and OctoPrint use shared read-back state; serial tool mapping
  is visible in Tune and the host UI throughout the job.
- Use a fixed-size status snapshot, one outstanding host poll and an
  allocation-free light timer. Regression validation: 51 firmware tests,
  432,308 assertions; plugin suite 198 tests with four skipped. Hardware
  two-screen validation and a long-print heap/stack soak are still required;
  this update is not a claim of verified crash-free operation on hardware.

- INDX serial jogs no longer rebase unknown XY to the corner opposite its
  X-min/Y-max home. If homing is invalidated, XY jogs and arcs require homing
  again instead of inventing coordinates. Other models retain their behavior.
- Restored per-print chamber, screen, and status-LED brightness controls in
  Tune on supported hardware. These use temporary overrides, leaving saved
  lighting defaults unchanged.
- Suppress routine nozzle-cleaning live-screen and host status messages.
  Routine cleaning/cleaned notices are also omitted from Messages history;
  cleaning failures remain visible.
- Preserve an existing RME lease during synchronous command processing,
  when the host cannot deliver its queued heartbeat.
- Automatic PA retains five-cycle wipe/ejection batches for all materials.
  PETG and other high-temperature materials cool for 12 seconds before ejection;
  lower-temperature materials retain four seconds.
  Cooling starts after wipe motion completes and the fan stays on through
  ejection. These timings require hardware validation for each material.

- Fixed post-print filtration countdown and early-stop state. Required cycles
  start their configured duration at print completion, including parked INDX
  tools; stopping a cycle cannot restart it on the next controller tick.
- Added Start/Stop Filter Cycle to Chamber Filtration, exposed Chamber Fans
  With Filter and Filter Fan Offset, and moved Empty Wastebin directly below
  Return in the Filament menu. Door prompts also work for manual cycles.

- Fixed the serial printing screen remaining on "Nozzle cleaning" after the
  operation finishes. Live status now looks through custom host messages to
  active operations instead of falling back to completed message history.
  Documented print-time RME session heartbeats required to retain the host icon.

- Fixed false INDX first-layer G2/G3 rejection: validate the actual directed
  arc sweep rather than requiring the entire supporting circle to fit.
  Endpoint and swept-extrema protection remain enabled; full circles still
  receive full-circle checks. Regression tests cover the reported N578 arc,
  reversed direction, unsafe extrema, full circles, and sampled sweeps.

- Corrected the remaining print-start bounds rejection after `G12 S30`.
  Serial Y-only purging is allowed within cleaner-local X +/-0.5 mm and
  Y 76..101.5 mm, using the calibrated cleaner origin and applied tool offset.
  Both endpoints must be inside this lane; lateral entry and Z moves do not
  receive this exception. The previous front-strip fix did not cover the
  cleaner position used by the slicer after bed probing.

- A parameterless `M105` is now valid while every INDX tool is parked. It
  returns the normal no-tool temperature snapshot without emitting
  `echo: Invalid extruder -1`, so OctoPrint does not incorrectly blacklist T0
  before a serial print selects its first tool. Explicit invalid `M105 Tn`
  requests remain errors.
- Fixed a deterministic CORE One/CORE One L INDX boot crash. The persisted
  nozzle-PID startup path used Marlin's raw `HOTENDS` count and attempted to
  construct a physical tool from the internal `NoTool` sentinel. PID loading,
  PID editing, automatic-PA target restoration, and RME tool-map reporting now
  iterate only strongly typed real tools; the sentinel is never advertised.
- Fixed tool-offset calibration exhausting the heap after its first recorded
  sweep. The large segmented load-cell sample buffer is now moved into the
  analysis result instead of being copied, and the result type is explicitly
  move-only so the firmware cannot silently reintroduce that allocation.
- Removed the second calibration heap spike identified after the recording
  fix: final position estimation now evaluates the trapezoidal sweep profile
  directly instead of allocating a 2,800-float lookup table while recorded
  load-cell samples are still resident.
- Reduced remaining analysis peak memory by filtering backward in place and
  compiling the rough-alignment score trace out of production images.
- Restored phase-stepping calibration heap headroom by removing the idle
  15 KiB automatic-PA sample buffer from permanent SRAM. M976 now acquires its
  bounded capture buffer only while measuring, releases it on every exit, and
  reports an explicit error if memory is unavailable.
- Fixed the remaining INDX phase-stepping heap crash identified from a 6.9.0
  crash dump. Spectral sweeps now evaluate their rectangular and Hann windows
  directly, preserving the analysis while removing all transient DFT-window
  heap allocations at the captured-signal memory peak.
- Added independent persisted INDX dock-calibration tolerances. X defaults to
  +/-2.5 mm and Y to +/-1.0 mm; both are adjustable from 0.5 through 5.0 mm on
  dock settings screens and through `@RME DOCK QUERY/SET`.
- Setup and control screens now remain at active brightness while non-print
  head or bed motion is running. Their normal inactivity timer begins after
  the operation completes, eliminating motion-time dim/bright oscillation.
- Removed the remaining heap-backed progress callback from tool-offset
  calibration. Its callback now uses bounded inline storage and is checked at
  compile time to remain allocation-free.
- Includes upstream 6.10.1 translations, welcome-screen corrections, and
  updated Prusa error-code definitions.
- Retains bounded text, bulk, and framed-binary parsing; durable resumable
  uploads; shared transfer ownership; and crash-dump storage.
- Retains asynchronous firmware-candidate validation and immediate cached
  firmware status responses.
- Retains independent filament material-family and profile reporting, M976
  material validation, and distinct extrusion-fault workflows.
- Retains session-scoped chamber-light hold and its local, print, disconnect,
  replacement-session, timeout, emergency-stop, and restart release paths.
- Fixed filament-menu tool selection on INDX and other mapped-tool systems.
  The selected virtual filament slot is now translated through the active
  tool map before emitting a `T` command, preventing a successful pickup from
  being falsely reported as "Failed to pick up the selected tool."
- Pickup validation now checks the attached physical nozzle rather than
  `active_extruder`. Before M701 loads filament, an INDX virtual slot is
  intentionally inactive, so the old check falsely rejected every nozzle.
- Restored the INDX interactive loading metadata sequence. Selecting a
  material now continues to color and manufacturer selection, and M701
  preserves both choices when committing the loaded virtual filament slot.
- Fixed a watchdog crash when a startup crash-dump export overlapped an RME
  firmware upload. Local and RME USB writers now share the transfer latch,
  and the bootloader cleanup marker is inspected once per media insertion
  after active storage work completes instead of reopening it every cycle.
- `RME_SESSION` now reports `active_tool=<logical-index|none>` from the live
  tool-selection state. INDX hosts can clear a stale selected tool as soon as
  every tool is parked instead of retaining the last virtual tool.
- Fixed the remaining INDX phase-stepping heap failure identified in the
  released 6.9.0 dump. Forward and backward accelerometer captures are now
  analyzed sequentially instead of being retained together, and peak
  detection no longer allocates a temporary candidate list.
- Crash-dump export now advertises its retained size and emits periodic busy
  and byte-progress responses, preventing a slow USB write from being mistaken
  for a failed RME command.
- Added conservative unhomed serial-jog boundaries for toolchanger machines.
  INDX assumes X at maximum and Y at zero to protect the purge bucket and
  front docks; XL assumes Y at maximum to protect its rear docks.
- Dock calibration now performs three complete, sensor-verified pick/park
  cycles at every measured dock before accepting it and advancing.
- INDX head LEDs now mirror the rendered center pixel of the front status bar,
  including configured colors, state brightness, and live animation frames.
- `G427 T0,7 R2 P3` can now declare the exact physical tools used by a serial
  print, avoiding the legacy all-enabled-tool fallback when file metadata is
  unavailable. Tool-offset purging also ejects the newly-created final pellet.
- Local and serial INDX tool-offset calibration now emit the documented
  `indx_tool_offset_calibration` phases and per-tool progress to RME clients.
- INDX automatic PA now uses the native prime position and a dedicated short
  push/reverse break-off motion before pellet ejection. This keeps curling
  strands out of the toolhead instead of using the long general-purpose scrub.

## Validation

- RME protocol/parser, transfer, Connect, and persistent-store test suites.
- CORE One INDX release link with compile-time enforcement of move-only sweep
  recordings and bounded progress callbacks.
- Complete final firmware matrix for every supported preset and translated
  MINI image.
- Per-target FLASH, RAM, CCMRAM, ISR-stack, firmware-descriptor, and backup-RAM
  linker limits.

The release assets are produced by the repository's final version build path
and carry the `6.10.1-RME` firmware suffix.
# RME spool-join control and calibration lighting

- Added `@RME SPOOLJOIN QUERY`, `ADD`/`SET`, and `RESET` so hosts can configure
  volatile spool-join chains, with structured errors and change notifications.
- Serial calibration commands now hold the configured active lighting state
  for their complete execution and release into the normal timeout afterward.
- A parked INDX toolchanger no longer reports extruder `-1` as an invalid tool
  when parameterless temperature, fan, or configuration commands are sent.
- Added structured RME dialog routing for INDX tool detection, dock/slot
  selection, pickup and park failures, dock calibration, nozzle-cleaner
  calibration, tool-offset calibration, and future generic INDX errors.
- Added guarded remote selection for the INDX unknown-tool dock picker with
  `@RME INDX SLOT SELECT slot=<zero-based-index>`.
- Fixed INDX M976 motion and extrusion safety. It now rejects stale, disabled,
  duplicate, or mismatched manifest mappings before motion; enters through the
  native cleaner keep-out route; purges between the prime block and wiper;
  wipes each pellet free; and exits without redundant dock cycling.
- Fixed a remaining INDX M976 collision path where legacy far-right bed cleanup
  overlapped the tool-8 dock. INDX automatic PA now performs no bed probing or
  bed cleanup, selects validated physical tools without virtual remapping, and
  wipes a strand before ejecting its pellet.
- Post-home and post-tool-change travel now stages through the native INDX
  cleaner approach, including its dock escape and segmented keep-out path,
  before any cleaner-local move.
- Ordinary homed INDX serial linear moves and complete arc envelopes now reject
  entry into tool-dock, cleaner, and purge-bucket keep-outs explicitly.
- Corrected the keep-out guard so Z-only/E-only commands remain valid while a
  native tool operation is in a service area, and ordinary non-service travel
  continues to use the existing software endstops.
- Unknown-but-detected INDX tools now report a waiting RME workflow so a host
  can acknowledge detection and then use guarded dock selection.
- The Josef Prusa (`pepa`) portrait is rendered in its original colors instead
  of being recolored by the selected UI theme.
- Host `echo:busy: processing` heartbeats now remain active when a long
  calibration intentionally suspends temperature/status auto reports. This
  prevents OctoPrint timeouts during INDX tool-offset, phase-stepping, homing,
  and related synchronous service operations.
- The INDX heatbreak fan remains temperature controlled when tools are parked:
  it runs above the nozzle/controller safety thresholds and for the existing
  two-minute cooldown, rather than following the logical active-tool field.
- INDX automatic PA now keeps consecutive high/low measurement cycles at the
  open waste-bin gap between the prime block and wiper instead of depositing
  on the block or traversing the cleaner after every cycle. It wipes and
  ejects after each five cycles to bound buildup, then always wipes and ejects
  the final remainder.
- Serial-print aborts no longer emit the media-only `Done printing file`
  marker, preventing OctoPrint from continuing with a desynchronized numbered
  command stream after a calibration failure.
- Corrected the INDX serial keep-out policy to allow straight Y-only entry and
  exit at the front priming strip. Lateral X travel there and entry into the
  cleaner remain blocked, so valid slicer priming moves no longer cancel with
  `Unsafe INDX move outside printable area`.
- INDX automatic PA now performs the native purge autoretract before its
  five-cycle wipe/eject operation, preventing a still-fed strand from following
  the nozzle and accumulating on the toolhead.
- Standard `M141`/`M191` chamber targets now use an independently heated bed
  on CORE One-family machines with a chamber sensor. Low-speed toolhead air
  circulation (plus CORE One L under-bed fans) spreads heat. Idle bed assistance
  is described above; active prints retain control of the bed target, and
  the existing xBuddy Extension cooling method remains unchanged.
- INDX automatic PA now runs the toolhead fan while a wiped purge pellet cools
  for four seconds before ejection. The pellet is solid enough to leave the
  waste bin instead of folding into it and merging with later purges.
- INDX serial keep-out validation now identifies lateral motion from the
  command's X field instead of transformed floating-point coordinates. This
  prevents valid relative Y-only purge moves from being falsely rejected after
  tool-offset or mesh transforms while retaining the cleaner hard boundary.

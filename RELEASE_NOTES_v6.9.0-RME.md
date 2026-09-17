# 6.9.0-RME Firmware

## INDX test update — door-open RME lighting priority, 2026-09-17

Updated INDX image only; download `coreone_indx_6.9.0-RME.bbf` again.
Other machine images remain unchanged.

INDX build passed from 717f7bedc; 58 RME cases / 432,403 assertions pass.

- With Door Holds Active enabled, an already-open door now cancels temporary
  RME Off/On overrides continuously. It uses the configured active profile;
  it does not enable every LED channel or force full brightness.
- RME chamber toggles no longer restart the shared idle countdown during an
  open-door hold. Closing the door starts the usual activity timeout.
- Locked mode, disabled door-hold behavior and the separate LCD control are
  preserved. Physical door/toggle validation remains pending.

## INDX test update — flow monitoring and cleanup, 2026-09-17

This update replaces only `coreone_indx_6.9.0-RME.bbf`. Download it again
even if you already have 6.9.0-RME. Other machine images remain the September
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
  432,364 assertions pass. INDX release build passes from source 319ef9f2a
  (66.90% flash, 77.47% RAM); no new heap allocations.
  Physical filament-break recovery, cancelled-print
  filtration, and cooled-pellet/main-wiper behavior require hardware testing.
  Hardware validation is pending; this INDX update is provided for testing.

## Build validation — 2026-09-16 chamber/motion update

All 15 release images passed; firmware source cfbac674e. Shared regression
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

6.9.0-RME ports the complete RME serial, file-transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack to upstream Prusa Firmware
Buddy 6.9.0. The upstream 6.9.0 INDX, nozzle-cleaner, UI, motion, persistent
store, and crash-dump changes are retained.

## RME integration changes

- Restore missing RME Settings links: lighting state profiles, chamber mode
  and timers, supported status/external lights, Serial Printing, Printer Lock,
  Heater Safety, PID, USB settings export, custom filament metadata and UI
  Theme. Hardware-specific guards remain; physical menu validation is pending.

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
- Fixed tool-offset calibration exhausting the heap after its first recorded
  sweep. The large segmented load-cell sample buffer is now moved into the
  analysis result instead of being copied, and the result is compile-time
  guarded as move-only. The progress callback also uses bounded inline storage.
- Removed the subsequent final-analysis allocation of a 2,800-float motion
  lookup table. Position estimation now evaluates the sweep profile directly
  while the recorded load-cell data remains resident.
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
- Fixed a deterministic CORE One/CORE One L INDX boot crash. The persisted
  nozzle-PID startup path used Marlin's raw `HOTENDS` count and attempted to
  construct a physical tool from the internal `NoTool` sentinel. PID loading,
  PID editing, automatic-PA target restoration, and RME tool-map reporting now
  iterate only strongly typed real tools; the sentinel is never advertised.
- Reconciled upstream's 6.9.0 serial-print lifecycle with RME streamed-print
  state, preventing stale finished states and numbered-command reset races.
- Retained separate filament material and profile telemetry. For example,
  Connect and RME report `material=PETG` independently from
  `profile=PET-00L`.
- Integrated upstream waste-bin cooldown, reheat, and safe return motion with
  RME's pause/resume workflow and host acknowledgement behavior.
- Integrated the session-scoped chamber-light hold with upstream's 6.9.0
  lighting controller and INDX head-light dimming.
- Preserved asynchronous firmware-candidate validation, durable resumable
  uploads, bounded parsers, shared transfer ownership, and crash-dump storage.
- Assigned the custom-filament color array a collision-free 6.9.0 journal key.
  Upstream added a calibration item whose hash collides with the former RME
  key, so custom color values reset once on upgrade instead of risking
  calibration or settings corruption.
- Fixed filament-menu tool selection on INDX and other mapped-tool systems.
  Virtual filament slots are translated through the active tool map before
  emitting a `T` command, avoiding false pickup-failure warnings.
- Pickup validation now checks the attached physical nozzle rather than the
  still-inactive virtual filament slot, allowing M701 to load that slot.
- Restored the INDX interactive loading metadata sequence. Selecting a
  material now continues to color and manufacturer selection, and M701
  preserves both choices when committing the loaded virtual filament slot.
- Fixed a watchdog crash when a startup crash-dump export overlapped an RME
  firmware upload. Local and RME USB writers now share the transfer latch,
  and the bootloader cleanup marker is inspected once per media insertion
  after active storage work completes instead of reopening it every cycle.
- `RME_SESSION` now reports `active_tool=<logical-index|none>` from the live
  tool-selection state. INDX hosts can immediately clear a stale selected tool
  when every tool is parked instead of retaining the last virtual tool.
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

- RME protocol, parser, firmware-status, upload, material/profile, M976,
  lighting-hold, stack-budget, and lifecycle tests.
- Shared transfer and Connect integration tests.
- Persistent-store journal collision generation.
- Complete final firmware matrix, including every translated MINI image and
  the feature-heavy CORE One INDX target.
- Per-target linker memory limits for FLASH, ordinary RAM, CCMRAM, ISR stack,
  firmware descriptor, and backup RAM.

The release assets are built with the repository's final signed RME build path
and carry the `6.9.0-RME` firmware suffix.
# RME spool-join control

- Added `@RME SPOOLJOIN QUERY`, `ADD`/`SET`, and `RESET` so hosts can configure
  the same volatile spool-join chains as the printer UI and `M864`.
- Added structured validation errors, unsupported-feature reporting, and
  `RME_CHANGE domain=spooljoin key=joins` notifications.
- Serial tool-offset, pressure-advance, phase-stepping, and INDX tool-offset
  calibration now hold the configured active lighting state until completion.
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

# 6.10.1-RME Firmware

## Summary

6.10.1-RME carries the complete RME serial, transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack forward to upstream Prusa
Firmware Buddy 6.10.1.

## Integration changes

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
  circulation (plus CORE One L under-bed fans) spreads heat without ever
  changing the bed target, active prints retain control, and
  the existing xBuddy Extension cooling method remains unchanged.
- INDX automatic PA now runs the toolhead fan while a wiped purge pellet cools
  for four seconds before ejection. The pellet is solid enough to leave the
  waste bin instead of folding into it and merging with later purges.
- INDX serial keep-out validation now identifies lateral motion from the
  command's X field instead of transformed floating-point coordinates. This
  prevents valid relative Y-only purge moves from being falsely rejected after
  tool-offset or mesh transforms while retaining the cleaner hard boundary.

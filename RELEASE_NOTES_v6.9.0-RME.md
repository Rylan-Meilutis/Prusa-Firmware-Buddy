# 6.9.0-RME Firmware

## Summary

6.9.0-RME ports the complete RME serial, file-transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack to upstream Prusa Firmware
Buddy 6.9.0. The upstream 6.9.0 INDX, nozzle-cleaner, UI, motion, persistent
store, and crash-dump changes are retained.

## RME integration changes

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
  calibrated prime-block position instead of traversing the cleaner after
  every cycle. It wipes and ejects after each five cycles to bound buildup,
  then always wipes and ejects the final remainder.
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
- Standard `M141`/`M191` chamber targets now support heatbed-assisted heating
  on CORE One-family machines with a chamber sensor. Low-speed toolhead air
  circulation (plus CORE One L under-bed fans) spreads heat, prior bed/fan
  settings are restored after assistance, active prints retain control, and
  the existing xBuddy Extension cooling method remains unchanged.
- INDX automatic PA now runs the toolhead fan while a wiped purge pellet cools
  for four seconds before ejection. The pellet is solid enough to leave the
  waste bin instead of folding into it and merging with later purges.

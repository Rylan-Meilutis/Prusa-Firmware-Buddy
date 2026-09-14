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

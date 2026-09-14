# 6.10.1-RME Firmware

## Summary

6.10.1-RME carries the complete RME serial, transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack forward to upstream Prusa
Firmware Buddy 6.10.1.

## Integration changes

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

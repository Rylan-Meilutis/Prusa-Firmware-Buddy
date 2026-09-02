# 6.9.0-RME Firmware

## Summary

6.9.0-RME ports the complete RME serial, file-transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack to upstream Prusa Firmware
Buddy 6.9.0. The upstream 6.9.0 INDX, nozzle-cleaner, UI, motion, persistent
store, and crash-dump changes are retained.

## RME integration changes

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

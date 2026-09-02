# 6.10.0-RME Firmware

## Summary

6.10.0-RME ports the complete RME serial, file-transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack to upstream Prusa Firmware
Buddy 6.10.0. Upstream 6.10.0 motion, INDX tool-offset wizard, UI animation,
cooling, persistence, and toolchanger changes are retained.

## RME integration changes

- Preserved the bounded RME text, bulk, and framed-binary parsers, durable
  resumable uploads, shared transfer ownership, and crash-dump storage.
- Reconciled upstream's virtual-tool-aware `M1601` recovery with the distinct
  RME runout, filament-not-moving, stuck-filament, and flow-limit workflows.
- Retained separate material-family and profile telemetry for RME and Connect.
- Retained asynchronous firmware-candidate validation and immediate cached
  firmware status reporting without blocking serial command processing.
- Integrated session-scoped chamber-light hold and local/print/session release
  behavior with the 6.10.0 GUI and lighting lifecycle.
- Adopted upstream's animated hourglass assets and printer display names rather
  than restoring obsolete resources from the earlier firmware line.

## Validation

- RME parser, firmware-status, upload/resume, material/profile, M976,
  lighting-hold, stack-budget, and lifecycle tests.
- Shared transfer, Connect integration, and persistent-store collision tests.
- Complete final firmware matrix, including every translated MINI image and
  the feature-heavy INDX targets.
- Per-target linker limits for FLASH, RAM, CCMRAM, ISR stack, firmware
  descriptor, and backup RAM.

Release assets are produced together with the 6.9.0 assets by the repository's
single final multi-version build command and carry the `6.10.0-RME` suffix.

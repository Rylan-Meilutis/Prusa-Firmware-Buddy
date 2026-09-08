# 6.10.1-RME Firmware

## Summary

6.10.1-RME carries the complete RME serial, transfer, firmware-update,
filament, lighting, workflow, and diagnostics stack forward to upstream Prusa
Firmware Buddy 6.10.1.

## Integration changes

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

## Validation

- RME protocol/parser, transfer, Connect, and persistent-store test suites.
- Complete final firmware matrix for every supported preset and translated
  MINI image.
- Per-target FLASH, RAM, CCMRAM, ISR-stack, firmware-descriptor, and backup-RAM
  linker limits.

The release assets are produced by the repository's final version build path
and carry the `6.10.1-RME` firmware suffix.

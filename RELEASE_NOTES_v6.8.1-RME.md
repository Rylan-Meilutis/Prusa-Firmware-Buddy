# 6.8.1-RME Firmware

## Summary

6.8.1-RME ports the complete RME firmware feature set to Prusa Firmware Buddy
6.8.1. It preserves the upstream 6.8.1 machine support and fixes the port
conflicts against its newer filament, GUI, storage, Connect, and INDX code.

## RME integration

- Out-of-band `@RME` sessions, structured events, workflows, dialogs, printer
  state, topology, usable build area, statistics, settings, themes, lighting,
  filament profiles, colors, manufacturers, tool mapping, and printer lock.
- Fast resumable USB file upload and download with framed binary and bounded
  text-bulk transports, CRC/SHA validation, disk-backed partial state, parser
  recovery, inactivity/disconnect suspension, explicit abort, and atomic
  publication.
- Shared RME/Connect/Link transfer ownership and print interlocks. Private
  `.rme-part` and `.rme-meta` files remain hidden from normal media and Connect
  listings but can be queried, resumed, or removed through the RME workflow.
- Protected, verified `FWUPD.RME` firmware staging with authoritative
  candidate/armed queries, idempotent unstage, and one-shot bootloader handoff.
  Ordinary `.BBF` files never become staged merely by existing on USB.
- Passive session keepalives and queries do not count as user activity, so
  idle lighting and deep-idle transitions continue normally.
- Complete saved four-state lighting matrix, temporary print overrides, RME
  transfer badge, external light-bar support, configurable timeouts, and
  corrected active/idle state reporting.
- Cause-specific recovery workflows for filament runout, filament movement not
  detected, extrusion flow limit, stuck filament, MMU, purge-bin, calibration,
  firmware update, and native load/unload operations.

## Filament and INDX

- Persistent material, color, and manufacturer synchronization, including
  complete custom filament profiles and loaded-tool assignments.
- Stationary load-cell pressure-advance calibration with bounded capture
  storage, machine-safe purge geometry, free-air INDX excitation over the
  bucket, and cleaner-wipe samples excluded from measurement.
- Optional fast INDX load-cell filament-presence and movement monitoring with
  guarded debounce, independent flow-limit handling, and accurate UI/RME fault
  reporting.
- Load, unload, MMU, and recovery operations suspend runtime pressure monitoring
  with nesting-safe restoration to prevent false positives.

## Compatibility and safety

- Includes upstream 6.8.1 behavior and machine definitions while retaining the
  RME serial-print UI, firmware updater, resource fonts, PID/heater safety,
  chamber/filtration controls, loaded-filament editor, and signing support.
- File payloads are streamed to storage rather than retained in heap memory;
  protocol buffers and queues are bounded to protect motion and GUI memory.
- Hosts should negotiate capabilities and follow
  [the protocol reference](doc/rme_serial_remote_protocol.md) and
  [the integration guide](doc/rme_serial_handler_integration.md). RME partial
  files must be managed only through the documented status/resume/abort/delete
  workflow, not through ordinary Connect media listings.

## Release policy

`v6.8.1-RME` is the persistent release tag for this upstream version. Updated
builds replace this release's notes and BBF assets in place; no per-build
release tags are created.

## Validation

- Built from the clean release worktree with
  `./build.py --final --versions 6.8.1 --jobs 15`.
- All 15 presets passed: CORE One, CORE One INDX, CORE One L, MINI and its
  eight language variants, MK4, MK3.5, and XL.
- `rme_protocol_tests`: 400,145 assertions across 15 cases.
- `transfers_tests`: 514,733 assertions across 11 cases.
- `connect_tests`: 277 assertions across 47 cases.
- Exactly 15 versioned BBFs were staged. The tightest flash target is MINI at
  97.33% for translated builds; the highest reported RAM target is CORE One
  INDX at 82.69%.

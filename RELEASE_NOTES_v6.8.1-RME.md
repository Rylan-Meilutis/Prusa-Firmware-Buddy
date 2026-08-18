# 6.8.1-RME Firmware

## Summary

6.8.1-RME ports the complete RME firmware feature set to Prusa Firmware Buddy
6.8.1. It preserves the upstream 6.8.1 machine support and fixes the port
conflicts against its newer filament, GUI, storage, Connect, and INDX code.

## RME integration

- Material telemetry now reports the base polymer family independently from
  the selected profile. Connect and Link expose `material=PLA` with a separate
  `profile=PLA-00D`, and loaded-filament serial reports use `S"PLA"` plus
  `P"PLA-00D"`; profile identifiers no longer masquerade as filament types.
  Base-preset storage and M865 `J` handling are enabled for every supported
  printer, rather than only the INDX variants.

- Fixed M976 batch validation and default PA fallback selection for custom
  filament profiles: slicer material fields now match the configured base
  material family (such as `PETG`) instead of the custom preset identifier.
  Exact custom names remain accepted as compatibility aliases.
- Made firmware-candidate status non-blocking. Verified uploads now persist a
  private size/SHA cache, while legacy candidates are hashed in bounded
  scheduler slices with `state=validating` progress, a 15-second storage-read
  deadline, and explicit read/hash/timeout errors. Serial G-code and RME
  traffic remain responsive throughout validation.

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
  corrected active/idle state reporting. Every application boot, including
  retained-crash export/error handling, starts with a bounded 30-second Active
  window using the saved Active brightness (subject only to its existing 15%
  readability floor), so an idle profile cannot leave the startup screen dark.
  RME writes, settings restore, and startup normalization enforce the same
  per-state screen limits as the printer UI.
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
- Connection-time RME frames remain zero-copy under the parser's recursion
  guard. This removes a 640-byte automatic Marlin-stack snapshot that could
  overflow immediately during the 6.8.1 session/query handshake.
- Durable upload resume preserves the partial and metadata when reopening or
  rehashing encounters a transient storage error instead of silently erasing
  it and restarting at zero. Restored bytes are also credited to the shared
  transfer monitor before new chunks are accepted.
- Resume metadata and path scratch no longer consume more than 800 bytes of
  Marlin task stack. Metadata is fsynced through a recoverable private
  `.rme-tmp` publication file, switching manifest jobs preserves unrelated
  durable checkpoints, and malformed action prefixes cannot execute commands.
- Crash-dump writes own the shared transfer latch, while queued M32/M997
  operations recheck it at execution to close Connect/Link overtaking races.
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
- `rme_protocol_tests`: 400,179 assertions across 18 cases.
- Instrumented RME parser/transfer core: 100% line coverage (172/172) and
  100% function coverage (27/27); branch coverage is 87.3% (144/165).
- Adjacent OctoPrint RME Compatibility serial-host suite: 103/103 tests,
  including binary upload, NACK recovery, abort, inactivity suspension,
  durable resume, transport fallback, and negotiated-capability checks.
- `transfers_tests`: 514,733 assertions across 11 cases.
- `connect_tests`: 278 assertions across 47 cases.
- Exactly 15 versioned BBFs were staged. The tightest flash target is MINI at
  97.53% for the Japanese translation; the highest aggregate RAM target is XL
  at 84.20%.

### Binary upload integrity follow-up

- Corrected the binary upload advertisement to a 512-byte, three-frame window
  whose complete wire backlog fits in the printer's 2048-byte CDC RX FIFO.
  The previous 1024-byte/eight-frame advertisement could not physically be
  buffered and caused dropped bytes, parser recovery, and corrupted uploads.
- Release builds made from linked worktrees now embed the exact 40-character
  commit hash in crash dumps instead of an unrelated branch ref.
- Guarded the live RME upload state. If memory corruption is detected, firmware
  now leaves raw mode and releases the shared transfer latch without
  dereferencing corrupted file/hash state; durable resume metadata is retained
  and the host receives `code=state_corrupt`.
- Binary recovery tests now call the same rolling header scanner used by the
  firmware. The adjacent OctoPrint RME Compatibility serial-host suite is also
  gated against the negotiated 512-byte, three-frame transport contract.

# RME 6.9.0 port audit

This audit records the version-specific safety work for the RME port from
6.8.1 to upstream 6.9.0.

## High-risk upstream overlaps

- Persistent store definitions and journal hashes
- Serial-print finalization and cleanup
- INDX waste-bin park, cooldown, reheat, and return motion
- Side-strip and INDX head-light state
- Filament menus, stuck-filament responses, and Connect telemetry
- Shared transfer pause/resume behavior
- Generated translations and constrained MINI resources

## Port decisions

- Upstream menu organization and generated translations are authoritative.
- RME's bounded out-of-band parser and serial lifecycle remain authoritative
  where emitting file-print completion markers would reset a live host's line
  numbering.
- Upstream waste-bin thermal and safe-return behavior is combined with RME
  pause/resume notifications; neither behavior is discarded.
- Material family and selected profile remain independent fields.
- The former `Custom Filament Color RGB` journal key cannot coexist with
  upstream's `Tool Offset Sensor Displacement` because both map to journal ID
  5777. The RME array uses `RME Custom Filament Color RGB` on 6.9.0. The old
  value is deliberately not migrated because its numeric identity is
  ambiguous in the combined schema.

## Required release gates

1. Build and run `rme_protocol_tests`, `transfers_tests`, `connect_tests`, and
   persistent-store tests.
2. Verify Connect INFO fixtures contain separate `material` and `profile`.
3. Run the journal hash generator through at least one real firmware preset.
4. Build the most constrained translated MINI preset and CORE One INDX before
   the full matrix.
5. Run `./build.py --final --versions 6.9.0 --jobs 15` and require every
   advertised preset to link within every memory region.
6. Publish only artifacts produced by that successful final matrix and verify
   their GitHub SHA-256 digests.

Passing host tests and linker limits substantially reduce regression risk but
do not replace an on-printer smoke test of boot, RME session open, upload
resume, serial print, and lighting hold.

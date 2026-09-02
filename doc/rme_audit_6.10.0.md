# RME 6.10.0 port audit

This audit records the version-specific safety work for the RME port from
upstream 6.9.0 to upstream 6.10.0.

## High-risk upstream overlaps

- Virtual-tool-aware `M1601` filament recovery
- INDX tool-offset calibration/wizard and toolchanger behavior
- GUI event processing and hourglass animation resources
- Persistent storage and constrained MINI resources
- RME session, lighting, transfer, and serial-print lifecycle hooks

## Port decisions

- Upstream 6.10.0 menu display names, animation resources, translations, and
  tool-offset wizard are authoritative.
- RME GUI processing remains connected to the upstream event loop.
- `M1601 V...` selects the requested virtual tool while `M1601 R...` retains
  distinct RME recovery causes; commands without `V` use the selected tool.
- Material family and selected profile remain independent fields.
- The bounded parsers, durable partial metadata, shared transfer monitor,
  asynchronous firmware validation, and lighting hold retain their audited
  6.9.0 behavior.

## Required release gates

1. Run the RME protocol, transfer, Connect, and persistent-store suites.
2. Build constrained translated MINI and feature-heavy INDX targets.
3. Run `./build.py --final --versions 6.9.0 6.10.0 --jobs 15` and require all
   advertised presets to link within every memory region.
4. Publish only artifacts from that combined successful build and compare the
   uploaded GitHub digests with local SHA-256 values.

Host tests and linker limits reduce regression risk but cannot replace an
on-printer smoke test of boot, session open, upload/resume, serial printing,
firmware status, and lighting hold.

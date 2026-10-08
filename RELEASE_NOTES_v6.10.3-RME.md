# 6.10.3-RME Firmware

The maintained RME release lines are now 6.9.2-RME and 6.10.3-RME.
This replaces 6.10.1-RME for this line; older releases remain available.

## Changes

- Integrate upstream v6.10.3: improved XL enclosure temperature estimation, heater-wait/resume handling, tool-offset probing and UART recovery fixes.
- Retain upstream XL bed-lowering behavior and existing RME INDX adaptations.
- Preserve RME serial printing, lighting, adaptive mesh, PA cache,
  flexible-material handling, SpoolJoin preparation and verified firmware identity.
- Companion RME Compatibility b121 includes manual GitHub firmware synchronization
  and lazy-loaded settings. Firmware discovery uses the included manifest;
  downloading an update never automatically flashes it.

## Availability and validation

- Release images: CORE One, CORE One INDX, CORE One L, MK4, MK3.5 and XL.
- All six release builds passed from `71f41c5f3`; linked application bytes and
  BBF payload lengths/hashes were verified. Later documentation commits do
  not change the binaries.
- MINI images are not included: the upstream integration exceeds the existing
  895 KiB application partition by approximately 3–4 KiB with the normal
  size-oriented compiler settings. Keep using 6.10.1-RME on MINI; do not flash
  another machine's image. No flash boundaries or features were removed to fit.
- Correct the PPS/PPA preset material-family IDs on high-temperature CORE One
  variants; the compile-time preset consistency checks remain enabled.
- RME source regression checks passed for serial pages, lighting lifecycle,
  INDX parking/E resets, load metadata, PA persistence and SpoolJoin preparation.
- Build success is not physical-printer certification. Tool pickup, calibration,
  flexible-filament extrusion and flashing still require hardware validation.

Prusa's official upstream 6.10.3 release targets XL/XL+.
RME adaptations are custom firmware; select the image matching your machine.
Included `rme-firmware-manifest.json` records verified BBF/application SHA-256
identities.

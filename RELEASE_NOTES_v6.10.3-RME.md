# 6.10.3-RME Firmware

The maintained RME release lines are now 6.9.2-RME and 6.10.3-RME.
This replaces 6.10.1-RME for this line; older releases remain available.

## Changes

### Updated release — 2026-10-10

- Status LEDs return to Idle/off after the configured finished hold expires,
  even if the Print Finished page stays open. Post-print filtration retains
  its indication, followed by the configured finished hold.

- Fix RME flashing getting stuck at “Looking for BBF”: startup resource
  installation now recognizes the exact staged `FWUPD.RME` filename while
  retaining compiled-resource digest validation and one-shot flashing.

- Preserve chamber/LCD/status Off across print completion until fresh user
  activity; RME On/door/touch restores Active and normal light cycling.
- Hold streamed host commands during parked pause and reheat/unpark recovery,
  preventing the reproduced 6.9.0-RME `E move without tool` resume crash.
- INDX pause docks the tool, synchronizes the shared E coordinate, then parks
  the empty head at X242/Y205 without changing the saved resume position or Z.
  Resume picks the saved tool, reheats, primes/wipes, returns to the saved
  position and only then releases the streamed job commands.
- Companion plugin: arm OctoPod snapshot substitution before FINISHING and
  retain the pre-final-bed-lowering frame rather than fetching after lowering.
- Companion RME b123 shows captured pause reasons only while paused, hides
  them on resume and clears them at job completion. Normal tool-change
  progress is no longer treated as a pause cause. Missing reasons remain
  explicitly marked unreported; genuine causes are logged for diagnostics.
- Companion RME b123 reduces background telemetry: controls every 10 seconds,
  changed progress every 5 seconds and unchanged progress every 20 seconds.
  Explicit control refreshes bypass the polling limit. Intermittent motion
  stalls are not claimed resolved without a hardware trace.
- Configure serial pause/resume scripts as `M601` / `M602` only; do not reset
  E or change positioning/driver modes during firmware-owned recovery.
- Updated application source: `c796dbc12`. Staged-image discovery regression
  passed (accepted canonical BBFs and exact FWUPD.RME; rejected sidecars and
  unrelated RME files). Native regression tests passed
  (67 cases, 432682 assertions), plus six lighting/recovery source checks.
  Physical camera timing and pause/resume still need hardware confirmation.

### Initial release

- Integrate upstream v6.10.3: improved XL enclosure temperature estimation, heater-wait/resume handling, tool-offset probing and UART recovery fixes.
- Retain upstream XL bed-lowering behavior and existing RME INDX adaptations.
- Preserve RME serial printing, lighting, adaptive mesh, PA cache,
  flexible-material handling, SpoolJoin preparation and verified firmware identity.
- Companion RME Compatibility b121 includes manual GitHub firmware synchronization
  and lazy-loaded settings. Firmware discovery uses the included manifest;
  downloading an update never automatically flashes it.

## Availability and validation

- Release images: CORE One, CORE One INDX, CORE One L, MK4, MK3.5 and XL.
- October 10 rebuild passed for all six variants from `f5389c476`, including
  application change `c796dbc12`. BBF embedded checksums, linked application
  bytes and manifest identities were verified. Later documentation commits
  do not change the binaries.
- Lighting lifecycle, serial resume and serial-page source checks passed
  (11 checks). Companion b123 passed 287 Python tests (four skipped) and nine
  UI suites.
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

# 6.9.2-RME Firmware

The maintained RME release lines are now 6.9.2-RME and 6.10.3-RME.
This replaces 6.9.0-RME for this line; older releases remain available.

## Changes

### Updated release — 2026-10-16

- Save color and manufacturer to the loaded slot after successful load/change/
  autoload. Preserve the selected color through Change Filament and autoload;
  purge-only operations do not replace metadata.
- Generate full compatibility-filtered lists only when a Change Filament
  picker opens, rather than synchronously building all tool lists on entry.
  Replace the preload color page before showing the manufacturer page.
- Expose Start/Stop Filter Cycle and filtration settings directly in Control.
  Emit filtration-close events when disabling the backend or restoring an idle
  snapshot, so hosts do not retain a stale running workflow. A touchscreen
  countdown that never expires still requires reproduction; this is not claimed
  resolved by the host event fix.
- Restore saved INDX loadcell detection preferences at each new job. M591
  queries now report actual runtime state; changing only S or U preserves the
  other runtime setting. Explicit M591 S0 still disables detection for that job.
- Host regressions cover metadata, menu transitions and detection settings;
  physical loading, responsiveness and filter timeout need hardware validation.
- Validation: final CORE One INDX build passed on both maintained branches;
  17 GUI source checks per branch passed. Shared native suites passed 22
  calibration cases (7284 assertions) and 67 protocol cases (432682 assertions).

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
- Updated application source: `ff6daf0d6`. Staged-image discovery regression
  passed (accepted canonical BBFs and exact FWUPD.RME; rejected sidecars and
  unrelated RME files). Native regression tests passed
  (67 cases, 432682 assertions), plus six lighting/recovery source checks.
  Physical camera timing and pause/resume still need hardware confirmation.

### Initial release

- Integrate upstream v6.9.2: restored PVA/BVOH presets, gantry squareness calibration (M1988), dock-calibration crash fixes and tool-parking/homing fixes.
- Preserve RME material-family EEPROM compatibility with upstream sparse preset indexing; remote reports retain stable preset slots.
- Preserve RME serial printing, lighting, adaptive mesh, PA cache,
  flexible-material handling, SpoolJoin preparation and verified firmware identity.
- Companion RME Compatibility b121 includes manual GitHub firmware synchronization
  and lazy-loaded settings. Firmware discovery uses the included manifest;
  downloading an update never automatically flashes it.

## Availability and validation

- Release images: CORE One INDX.
- October 16 rebuild passed (1/1) from `1cbde25b2`, including the filament
  workflow and filtration reporting fixes. Later documentation commits do
  not change binaries.
- Lighting lifecycle, serial resume and serial-page source checks passed
  (11 checks). Companion b123 passed 287 Python tests (four skipped) and nine
  UI suites. BBF embedded checksums and manifest identities were verified.
- RME source regression checks passed for serial pages, lighting lifecycle,
  INDX parking/E resets, load metadata, PA persistence and SpoolJoin preparation.
- Build success is not physical-printer certification. Tool pickup, calibration,
  flexible-filament extrusion and flashing still require hardware validation.

Prusa's official upstream 6.9.2 release targets CORE One INDX.
RME adaptations are custom firmware; select the image matching your machine.
Included `rme-firmware-manifest.json` records verified BBF/application SHA-256
identities.

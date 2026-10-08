# 6.9.2-RME Firmware

The maintained RME release lines are now 6.9.2-RME and 6.10.3-RME.
This replaces 6.9.0-RME for this line; older releases remain available.

## Changes

### Updated release — 2026-10-08

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
- Companion RME b122 shows captured pause reasons above OctoPrint's progress
  bar and in RME, preserving the last reason for the current job. Missing
  reasons are explicitly marked unreported.
- Configure serial pause/resume scripts as `M601` / `M602` only; do not reset
  E or change positioning/driver modes during firmware-owned recovery.
- Updated application source: `479664b9c`. Native regression tests passed
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
- Built source: `92153229c`. Later documentation commits do not change binaries.
- RME source regression checks passed for serial pages, lighting lifecycle,
  INDX parking/E resets, load metadata, PA persistence and SpoolJoin preparation.
- Build success is not physical-printer certification. Tool pickup, calibration,
  flexible-filament extrusion and flashing still require hardware validation.

Prusa's official upstream 6.9.2 release targets CORE One INDX.
RME adaptations are custom firmware; select the image matching your machine.
Included `rme-firmware-manifest.json` records verified BBF/application SHA-256
identities.

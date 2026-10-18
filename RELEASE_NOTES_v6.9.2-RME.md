# 6.9.2-RME Firmware

The maintained RME release lines are now 6.9.2-RME and 6.10.3-RME.
This replaces 6.9.0-RME for this line; older releases remain available.

## Changes

### LCD filtering redraw correction — 2026-10-18

- Only apply LCD control/brightness writes when the brightness changes; do not
  repeatedly clear an already-off panel during filtering/idle LED updates.
- Keep status-strip animation updates and the LCD off mask active, with normal
  off/wake transitions. This update does not change PWM resolution.
- Executable stub-driver tests cover ST7789 and ILI9488 repeated brightness,
  off and wake behavior. Physical header/fade behavior needs hardware verification.
- RME Compatibility 0.1.0b128 remains compatible; no plugin update is required.

### Spool-mapping correction — 2026-10-17

- Preserve complete per-tool loaded filament parameters before a shared user
  preset is replaced during spool synchronization. Updating one slot no longer
  silently converts another loaded tool from PLA to ASA.
- Matching plugin assigns native New Spool saves to the originating slot,
  refreshes inventory immediately, retains staged mapping choices, and handles
  an eighth distinct spool with per-tool storage instead of dropping it.
- Regression checks include the reproduced T7 shared-preset case. Existing
  incorrect mappings require explicit correction; no physical material is guessed.
- Matching RME plugin: 0.1.0b125. Validation: 302 Python tests (four skipped),
  ten JavaScript UI suites, 69 native protocol tests, and 51 GUI source checks
  per maintained firmware branch (one skipped). Physical spool mapping requires
  verification on the updated printer.

### Updated release — 2026-10-17

- Extend live controls to bed/nozzle temperature, print fan, speed and flow;
  keep touchscreen non-motion setters and telemetry active in nested waits.
- Reject unsupported embedded BBF machine/version combinations at selected-file
  M997 handoff, in addition to matching plugin upload/staging/flash checks.
- Matching plugin fixes automatic pre-bed-lowering snapshots for Orca object
  markers and reuses the cached frame for delayed OctoPod completion notifications.

Validation: the supported 6.9.2 INDX and 6.10.3 XL release builds pass,
as do 44 GUI source regression tests per branch. The matching plugin passes
299 Python tests (four skipped) and nine UI suites. Physical load/unload pellet
ejection, live control response and LCD recovery behavior require hardware checks.

- Own picker label text so custom color/manufacturer names do not reference
  temporary profiles. Clear LCD memory before display-on during recovery,
  retain brightness, and log recovery checks; idle flashing needs hardware validation.
- Filament handling starts/restarts configured filtration without a door-open
  early-exit prompt. Print-end/manual filtration retains its prompt behavior.
- Serial pause/resume/cancel now displays the same transient states as local
  print controls. Routine Messages pages remain suppressed.
- Poll bounded serial recovery input even when the normal FIFO is full so
  lighting controls remain reachable during temperature waits.
- Touchscreen chamber targets use a thermal mailbox instead of queued G-code;
  M191 follows changed targets and exits when chamber control is switched off.
  RME advertises live_chamber=1 for matching plugin support.
- INDX load purges and unload cleaning explicitly eject the pellet after wiping,
  using calibrated cleaner sequences without adding extrusion or changing TPU speeds.
- Restrict build presets to verified upstream machine support. 6.9.2 supports
  INDX; 6.10.3 supports XL, not INDX or other previously cross-built models.

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

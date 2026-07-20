# RME Upstream Release Playbook

Use this when Prusa publishes a new Firmware Buddy release and the RME firmware needs to be rebuilt quickly on top of it.

## Goal

Bring the RME feature set onto the new upstream release with the smallest practical diff, build all supported firmware images, and produce release notes and BBFs on day one.

The top-level wrapper holds `.dependencies/.rme-build.lock` for its complete
lifetime. Multi-version children inherit that ownership. If another invocation
reports the lock as busy, let the owning build finish or terminate it cleanly;
Ctrl-C, process exit, a crash, or reboot releases the kernel lock automatically.
The lock file may remain and does not itself block builds; do not remove it to
force overlapping Ninja/output operations.

The PA calibration port includes both slicer-driven M976 and the manual
Control-menu screen immediately above Calibrations & Tests. Verify its loaded-tool toggles, per-tool temperature controls, single Run action, whole-batch blocking progress FSM, aggregated results, Abort fallback, USB result export, persistent chamber lighting, and MMU unload-before-each-probe/final-unload ordering. When rebasing, verify physical-tool and MMU-slot
selection of loaded filaments only, automatic material-profile temperatures,
the ±15 °C manual-temperature safety bound, sequential batch submission, the manual-only clean-area prompt,
anchor acknowledgement, probe-before-full-heat ordering, 10 mm hotend/sheet clearance, scoped filament-sensor event locking, and restoration of every prior hotend target after all
M976 exit paths.

Verify the root Filament menu is the only loaded-filament entry point. Each
loaded tool row must show material and color name with an outlined visual swatch, its editor must stage
Material and Color independently and commit only on Save, and the color chooser
must omit unused user slots. On non-MINI printers, verify Settings > Filament Colors creates a named
color through the color-value picker and that `M865` retains the exact hex value
for serial hosts while the loaded-filament list does not display preset metadata.

Verify Settings > FW update opens a page containing Select BBF from USB and
Update Instructions. The picker must reject non-BBF files and paths outside
`/usb`, but accept completed Connect downloads in USB subdirectories and long
filenames. After confirmation it must copy the selection to `/usb/FWUPD.BBF`,
flush it, hand that short filename to the bootloader, and restart without
removing the source download. A `/usb/FWUPD.UI` marker must cause the application
to remove the temporary BBF and marker on the next USB mount after the bootloader
attempt; an unmarked serial-uploaded BBF must not be removed. Test copy failures, invalid-signature, and wrong-model
files to ensure the bootloader rejects them without flashing. Serial regression
coverage must retain `M997 O` and `M997 /usb/FIRMWARE.BBF`; the latter selects a
file already present on USB rather than carrying the file data itself.

RME's binary upload layer is `M998`: verify begin rejects zero/oversized files
and malformed SHA-256, chunks reject invalid Base64 and any offset other than
the last acknowledged byte, finalization rejects size/hash mismatch, and abort
removes `/usb/FWUPD.TMP`. A successful finalization must atomically expose
`/usb/FWUPD.BBF` without rebooting; only a later `M997 /usb/FWUPD.BBF` may
request flashing. Exercise media-removal and write-failure paths as well as a
successful signed-BBF upload.

MINI must retain the same firmware-update menu and M998 validation/state
machine as MK/CORE builds. Its 7x13, 9x16, and 11x18 bitmap fonts belong in
`/internal/res/fonts` and are generated from the selected complete font headers
as independently compressed random-access glyph records in the resources
tarball. The 6.5.7 branch must retain the backported tarball extractor rather
than its fixed-size legacy LittleFS BBF image. Verify Japanese, Ukrainian, and Latin MINI presets,
including resource replacement after an update, missing/corrupt-resource
behavior, repeated-glyph caching, and that no embedded font arrays return to
the application link map. Keep meaningful internal-flash headroom; do not
remove update UI or language coverage to make a MINI image link.

## Current Baseline

RME 6.5.7 is based on upstream tag `v6.5.7` at `7119a302d6`.

The active release branch is:

```sh
rme-v6.5.7
```

The release notes for the active release are:

```sh
RELEASE_NOTES_v6.5.7-RME.md
```

The RME patch stack was originally audited from the 6.5.3 branch. The first source commit by Rylan Meilutis is:

```text
706eb6ea5d25403a8924b82e29f3a23786fe594c new octoprint screen and better color scheme
```

The audit baseline for this playbook is the parent of that commit:

```text
3fc7b43a3b9bd77e9267647af5d96fe1ee7cb1c2
```

To re-audit the full RME patch surface on the active release branch:

```sh
git diff --stat v6.5.7..rme-v6.5.7
git diff --name-status v6.5.7..rme-v6.5.7
git diff --dirstat=files,0 v6.5.7..rme-v6.5.7
```

At the time this playbook was last audited, the RME 6.5.7 release port covered 535 files with 13,284 insertions and 1,220 deletions relative to upstream `v6.5.7`. The stack contains 116 replayed RME commits through `f3c3ec13e`, plus the `Finalize 6.5.7 RME release port` commit for the status LED state merge, release documentation, and generated 256-field config-store visitor support. The largest categories are GUI resources/theme assets, serial printing, LED/light-bar behavior, build tooling, safety/chamber logic, config-store additions, PID management, Prusa Connect compatibility, and GUI framework polish.

The original 6.5.3 source patch range remains useful for archaeology and conflict comparisons:

```sh
coreone-v6.5.3-patches
git diff 3fc7b43a3b9bd77e9267647af5d96fe1ee7cb1c2..coreone-v6.5.3-patches
```

## Supported Release Targets

Build these presets for a full release:

```sh
coreone
coreonel
mini
mini-en-cs
mini-en-de
mini-en-es
mini-en-fr
mini-en-it
mini-en-pl
mini-en-ja
mini-en-uk
mk4
mk3.5
xl
```

Expected BBF output directory:

```sh
bbf
```

## One-Day Update Flow

1. Fetch upstream tags and create a fresh integration branch.

```sh
git fetch upstream --tags
git switch -c rme-vX.Y.Z upstream/vX.Y.Z
```

If the upstream remote is named differently, check with:

```sh
git remote -v
```

2. Replay the RME patch stack.

Prefer a linear cherry-pick of the existing RME commits from the last release branch. Keep upstream changes intact unless they directly replace an RME patch.

```sh
git log --oneline v6.5.7..rme-v6.5.7
git cherry-pick <first-rme-commit>^..<last-rme-commit>
```

If the old branch has extra experimental commits, cherry-pick the feature groups below instead of the whole range.

3. Before resolving conflicts, regenerate the changed-file inventory from the old release branch and compare it to this playbook.

```sh
git diff --name-status v6.5.7..rme-v6.5.7 > /tmp/rme-files.txt
git diff --dirstat=files,0 v6.5.7..rme-v6.5.7
```

Every non-resource source directory in that inventory should map to one of the feature groups below. If a file does not fit, add a new playbook item before finishing the rebase.

4. Resolve conflicts by feature area. Do not resolve mechanically; upstream often changes GUI, FSM, and Marlin integration code between releases.

5. Run the narrow compile checks early.

```sh
./build.py --preset xl --bootloader yes
./build.py --preset coreone --bootloader yes
./build.py --preset coreonel --bootloader yes
./build.py --preset mini --bootloader yes
```

XL is the side-LED/enclosure gate. MINI is the layout and small-display/screen-only brightness gate. Core One and Core One L are the chamber/door/LED/resource-image gates. MK4 or MK3.5 is the non-side-LED status/display brightness gate.

6. Run the full release build after focused targets pass.

```sh
./build.py
```

The top-level wrapper defaults to at most four concurrent printer builds. Keep that default for normal release builds to avoid overwhelming the build machine. Use `--jobs N` only when the machine has been sized for a different level of parallelism. If the wrapper is interrupted, it terminates active child builds so Ninja/LTO processes do not remain orphaned. Preserve the final per-machine summary with flash usage, aggregate RAM usage, individual memory-region usage, total elapsed wall-clock time, and absolute staged BBF paths.

The wrapper prepends the managed virtualenv bin directory to child build `PATH` and passes `Python3_ROOT_DIR` by default so nested CMake projects can find Nunavut `nnvg`. If a custom Python root is required, override it explicitly:

```sh
./build.py --final --jobs 4 -D Python3_ROOT_DIR:PATH=/path/to/venv
```

For broad compatibility after touching shared CMake, resources, build options, or feature flags, also run the default Buddy preset matrix from `utils/build.py` on at least one clean machine:

```sh
python3 utils/build.py --bootloader yes
```

The default matrix includes additional Buddy-enabled targets and sub-board firmware beyond the focused BBF release list. Use it as a compile-compatibility gate, then stage only the BBFs intended for the release.

7. Update the release notes with the new upstream version, upstream tag/commit, RME feature list, known limitations, and build results.

8. Stage BBFs from `bbf/` and verify every expected preset produced a matching `.bbf`.

## Bootloader And Signing Boundary

The RME firmware targets the stock Prusa bootloader, but the bootloader itself is not part of the open-source firmware patch surface and is not built from this repository.

Keep these release rules intact:

```text
Build firmware images with the normal `--bootloader yes` packaging path when release BBFs need to target printers using the stock bootloader.
Do not attempt to port or modify Prusa bootloader behavior as part of the RME feature stack.
Do not claim custom signatures make RME builds genuine to the unchanged stock bootloader.
Document any signing key as a custom/private trust-chain aid only.
Expect the official non-genuine firmware warning on stock bootloaders.
```

## Feature Groups To Preserve

### Serial Printing

Keep the dedicated serial print screen and serial host state handling.

Important areas:

```text
src/common/serial_printing.*
src/gui/screen_printing_serial.*
src/gui/screen_menu_serial_printing.*
src/gui/screen_messages.cpp
src/gui/window_print_progress.cpp
src/common/marlin_server.cpp
src/common/marlin_server.hpp
src/common/feature/print_status_message/
src/gui/dialogs/DialogHandler.cpp
src/marlin_stubs/pause/M600.cpp
src/marlin_stubs/pause/pause.cpp
src/marlin_stubs/sdcard/M20-M30_M32-M34.cpp
src/marlin_stubs/host/M115.cpp
lib/Marlin/Marlin/src/gcode/queue.cpp
src/mmu2/mmu2_reporting.cpp
```

Behavior to verify:

```text
M75 or eligible M73 starts serial print state.
Blocking heater waits such as M109, M190, and M191 can start serial print state when automatic detection is enabled.
Homing, mesh-leveling, and ordinary toolhead movement commands do not start serial print state by themselves.
M77 ends serial print state. Do not treat progress-only M73 P100/Q100 R0/S0 as a hard end marker; it can arrive before streamed end G-code has completed.
M77 received from serial must finalize the serial print before it enters the normal G-code queue, return `ok`, keep the persistent print-finished screen active, and leave later serial commands accepted.
Serial print finalization must clear `GCodeQueue::pause_serial_commands`; otherwise a following M75 or host command after print completion can be ignored.

Serial-print start must snapshot the print-status message ID before the triggering command executes. The serial UI must ignore records at or below that session baseline and show the neutral waiting page until the current job provides a firmware status, host message, progress, or ETA. This prevents stale temperature/probing state from a preceding job appearing during an otherwise idle host startup.

Firmware-side serial status reporting uses rate-limited `//action:notification` lines while a serial print is active. Preserve coverage for heater waits and heat soak, homing and probing, MMU load/unload/change progress, tool-change progress, filament runout, and translated MMU/tool-change errors. Do not emit these notifications for non-serial jobs, and keep repeated progress reports throttled to avoid disrupting host command flow.
M75, startup M73, and OctoPrint startup messages received while a serial print is already active must not arm a pending second print start. End G-code sent after M77 must not restart serial print state or clear the frozen completed-print duration.
Ignored short serial macro prints, such as a quick M75/M77 pair with no printed Z height and less than the minimum useful duration, must restore the previous Marlin print state, active print screen, and LED state. If the previous state was a persistent finished screen that was still visible, keep that finished screen and green finished indication; if no print FSM was visible, return to idle/home behavior.
Canceling a detected serial print before its initial home must not issue an unhomed Z clearance move when no printed Z height exists. Shut down heaters and continue abort cleanup without moving the steppers.
Serial-origin M601 must stop serial intake, drain queued moves, park the head, keep the print in the active paused print lifecycle, then report `//action:paused firmware_pause`. Do not echo a fresh `//action:pause` back to the same serial host that sent M601, because OctoPrint can recursively run host pause scripts and turn heaters off.
Serial-origin M602 must resume through the firmware unpark/reheat path and report `//action:resumed` after the printer is actually continuing. Do not echo a fresh `//action:resume` back to the same serial host that sent M602.
MMU/runout and other firmware-initiated manual-intervention recovery sends the host resume action and then reports resumed after the printer is actually continuing.
When an MMU, side, or xBuddy-extension filament sensor is the primary print-time runout source, keep the toolhead/extruder sensor active as a secondary runout source so broken or stuck filament between the upstream sensor and extruder still triggers the normal M600 pause. Avoid duplicate runout injection by skipping the secondary check when both logical sensors resolve to the same physical sensor.
Loadcell stuck-filament detection is controlled by `M591 S` and the `Stuck Filament Detection` GUI toggle on loadcell printers. Filament movement/tangle protection is controlled separately by `M591 U` and the `Filament Movement Detection` GUI toggle. Both settings share the loadcell E-stall detector and M1601 pause path, so either enabled setting keeps the detector armed. Preserve `M591 S/U/P/R` behavior so users can enable, persist, and restore defaults without requiring a rebuild.
Serial MMU print finalization must not rely on GCodeInfo single-tool metadata, because streamed jobs often do not have scanned file comments. If the MMU still reports filament present, firmware finalization must attempt the MMU unload so OctoPrint-style jobs do not leave filament in the extruder.
Firmware/manual-intervention pause states during serial prints should report paused to the host, keep bed heat protected from the bed safety timer, allow nozzle safety handling where appropriate, restore the nozzle target before resume when needed, then send host resume/resumed actions so OctoPrint and similar hosts do not remain paused.
MMU reporting hooks must only defer host pause/resume events. Do not call `SerialPrinting::*` or `buddy::safety_timer()` directly from the MMU reporting callback; consume the deferred events from the Marlin server loop where the safety timer and serial host actions are safe to run.
Fresh, useful host status progress/ETA, such as OctoPrint-plugin M117 status text, takes precedence over streamed G-code M73 progress so percent/ETA do not jump backward. Host ETA snapshots count down locally between messages. Repeated startup-style 0% messages and unrecognized ETA text leave streamed M73 P/R fallback data in control.
Serial UI mode is a dropdown: Legacy, Messages Only, Progress.
Legacy mode shows the OctoPrint-style logo.
Messages Only shows the fullscreen message view.
Progress mode shows the new progress/status/message UI.
The serial message page filters progress/ETA/time-only host text.
Clock-only messages with today/tomorrow suffixes are filtered.
The message page shows a numeric print percentage.
Load/unload, MMU purge, nozzle-cleaning recovery, safety-timer resume, and cold-pull progress can drive the serial status progress page when active.
Message-screen time suppression is also present in the general Messages screen, not only the serial print screen.
M115 advertises printer storage support for OctoPrint (`SDCARD`, `EXTENDED_M20`, `LFN_WRITE`).
M20/M21/M23-M30/M32 support OctoPrint printer-storage workflows: list, upload, select, print-from-SD, pause/resume, progress status, seek, and delete.
M28/M29 upload lines are written to USB media and are not passed through the serial print detector as live print commands.
Serial M28 must enter upload mode immediately from the serial receive loop. Do not leave it queued behind incoming file lines, because fast OctoPrint uploads can otherwise be acknowledged without being written to USB.
Finished serial-print summaries persist until acknowledgement or the next print and rotate duration, completion time, and remaining filtration time when active.
Host paths normalize onto `/usb/`, reject parent-directory traversal, and create upload directories as needed.
```

### Prusa Connect Compatibility

Keep the Connect-facing version compatible with the upstream release while preserving RME branding everywhere else.

Important areas:

```text
src/connect/printer_common.cpp
src/connect/connect.cpp
src/connect/registrator.cpp
src/common/http/httpc.*
src/connect/render.cpp
src/connect/marlin_printer.cpp
src/connect/planner.cpp
```

Behavior to verify:

```text
Connect registration, telemetry/events, and websocket upgrade requests report the base upstream firmware version through the Connect JSON `firmware` field and `User-Agent-Version` header.
The printer UI, M115, local PrusaLink `/api/version`, BBF packaging, crash-dump build identification, transfer recovery, metrics, and splash/version screens continue to use the full RME version where they already did.
Connect info events advertise writable storage only when USB media is inserted and mounted. The `storages` array must include `/usb`, `read_only: false`, and free-space data before Connect can upload files.
Connect download commands remain accepted through `START_INLINE_DOWNLOAD`, `START_CONNECT_DOWNLOAD`, and `START_ENCRYPTED_DOWNLOAD`.
Connect downloads stay constrained to `/usb` and transferable file types.
Serial-printing UI FSM state must not mask the underlying Marlin print lifecycle from Connect. During serial prints, Connect should see normal `PRINTING`, `PAUSED`, `ATTENTION`, `FINISHED`, and `STOPPED` states rather than generic `BUSY`.
Do not remove the RME suffix globally as a workaround for Connect; keep the compatibility version scoped to Connect-facing reporting.
Connect chamber-light telemetry must report the currently driven RGBW brightness, not the configured active-state maximum. If the external GPIO chamber-light output is actually energized, report the combined chamber light as 100% even when RGBW brightness is 0%. A `ChamberLedIntensity` value received from Connect is a wake/activity command only and must not modify any persistent or per-state brightness setting.
```

### LEDs, Chamber Lights, Side Strips, And GPIO Light Bar

Keep per-state lighting, temporary per-print percent overrides, status LED separation, chamber-light acknowledgement behavior, and the timed status-LED finished hold.

Important areas:

```text
src/leds/
src/gui/screen_menu_lights*
src/gui/screen_menu_tune*
src/gui/menu_item/specific/
src/common/config_store/
src/persistent_stores/store_instances/config_store/
src/gui/screen/screen_menu_led_state.*
src/gui/screen/screen_menu_leds.hpp
src/gui/screen_menu_led_colors.*
src/gui/screen_menu_external_light_bar.*
src/gui/screen/screen_menu_external_light_bar.hpp
src/gui/screen/screen_chamber_filtration.hpp
src/gui/MItem_tools.*
src/gui/MItem_menus.*
src/guiapi/src/gui.cpp
src/gui/MItem_hardware.cpp
src/marlin_stubs/M151*
src/marlin_stubs/M152*
src/marlin_stubs/M153*
src/marlin_stubs/M154.cpp
src/marlin_stubs/M150.cpp
src/marlin_stubs/M262-M268.cpp
src/marlin_stubs/PrusaGcodeSuite.hpp
src/marlin_stubs/gcode.cpp
src/common/feature/xbuddy_extension/
src/device/stm32f4/hal_msp.cpp
src/device/stm32f4/peripherals.cpp
lib/Marlin/Marlin/src/feature/twibus.cpp
src/module/leds/led_animation_controller/include/led_animation_controller/
```

Behavior to verify:

```text
Core One chamber LED off does not turn off status LEDs/status bar.
Temporary chamber/side brightness is percent-based on every printer with `HAS_SIDE_LEDS`.
Temporary status-strip brightness is percent-based on every printer with `HAS_LEDS`, including Core One variants, XL, MK4, and MK3.5.
Persistent LED enable and brightness values are independent between Deep Idle, Idle, Active, and Printing. Do not add cross-state ordering clamps: Printing exterior LEDs may be dimmer than Idle LEDs or fully off while Idle remains enabled. The only state brightness floor is the 15% LCD minimum for Active and Printing screen brightness. Preserve `M153` for incidental non-print host activity, but ignore it while a print is active or paused.
Temporary print lighting overrides reset after the print. A chamber/side LED override of 0% must not change the logical side-strip state to off or affect LCD brightness. Keep the logical print state active and render only the LED output at 0% brightness.
Per-state screen brightness is available on supported displays for Deep Idle, Idle, Active, and Printing.
Active and Printing screen brightness settings are clamped to at least 15% in both the UI range and stored value.
Idle and Deep Idle screen brightness can be set to Off/0%.
Off/0% screen brightness should write a full black frame to the panel before using the no-brightness-control/backlight-disable command and display-off. ST7789/MINI should also enter sleep-in, then sleep-out and display-on when waking. ILI9488/XLCD should also force the front/status LED strip dark. Display-driver pixel writes must be suppressed while the screen is intentionally off so delayed UI draws cannot repaint the old UI. Non-zero brightness must re-enable display writes and brightness control before setting the brightness value, and the off-to-on transition must force a full-screen redraw.
If idle/deep-idle screen brightness is below 15%, the first touch or encoder input only wakes/brightens the screen and must not trigger the focused action.
If temporary print screen brightness is below 15%, the first touch, encoder input, or Core One door wake raises the screen to 15% and consumes only that wake input. Subsequent input must remain responsive while the print override is still below 15%, and the wake floor must clear when the print ends or the override changes.
Bootstrap/install screens must hold screen brightness at 100% until `TaskDeps::Tasks::bootstrap_done` is satisfied.
The splash screen must be painted before `gui_display_ready` is provided, and the initial idle/activity timer must be seeded only after the first home-screen paint.
`Active to Idle` is measured from last activity to idle entry.
`Idle to Deep Idle` is measured from idle entry to deep-idle/off entry, not from the original activity timestamp.
Canceled/aborted prints show abort indication until door open/close acknowledgement or new print.
Status LEDs smoothly pulse from the configured warning color toward RGB white as the post-print filtration timer expires unless the completed print is acknowledged by a Core One chamber-door open/close cycle. Apply the filtration-progress tint after rendering the pulse envelope so the changing tint does not restart the animation. Keep this visually distinct from the solid-green finished hold, and do not add a fully-off dwell between the fade-down and fade-up portions of the pulse.
A running filtration fan must not select the green filtering animation during an active print. Keep the normal blue printing animation until the printer reaches `Finished`.
A Core One chamber-door acknowledgement during filtration suppresses both the remaining filtering blink and the later solid-green status hold until the next print starts.
Do not accept chamber-door acknowledgement before the printer reaches `Finished`, including during finishing park and unload phases.
Opening the Core One chamber door during post-print filtration opens an end-filtration prompt. The prompt closes automatically if filtration finishes naturally while it is open and returns to the persistent print-finished screen.
Closing the Core One chamber door while the end-filtration prompt is open must dismiss only that prompt. It must not acknowledge or close the underlying persistent print-finished screen.
The persistent print-finished screen exposes `Stop Filter` while filtration remains active. That action ends filtration without dismissing the completed-print summary; `Continue` or `Home` remains independent.
Acknowledging the persistent print-finished screen through `Continue`, `Home`, or `print_exit()` must reset the status LED finished/filter indication centrally so the next idle/active state is not left latched.
Serial print-finished summaries expose page dots and swipe navigation for elapsed duration, completion time, and remaining filtration time when available.
Capture elapsed print duration before abort cleanup begins. Cleanup G-code may restart and reset the stopwatch; canceled prints must retain the pre-cleanup duration.
Continuously preserve the maximum elapsed print duration while a print is active. Serial hosts may issue timer stop commands before firmware finalization, and both serial and file-print finished summaries must render the preserved duration rather than a reset stopwatch value.
Print duration includes startup heating. Keep timer startup in `PrintInit` and `SerialPrintInit` before blocking heater waits continue, and keep `_server_print_loop()` running from nested `cycle()` calls during `M109`, `M190`, and `M191`.
Update serial-print header captions only when the print state changes. Reapplying an unchanged caption during each GUI loop invalidates the full header and causes visible flicker.
Switch serial-print message pages by invalidating the affected child widgets through their visibility changes. Do not invalidate the entire screen when entering or leaving the message page.
The acknowledged-filter path must force the status LED output fully black while filtration remains active; selecting the idle animation alone is insufficient because idle brightness and color may be configured non-zero.
Built-in UI theme selection applies the matching status LED palette immediately. Keep the Indigo printing status bar at the light-bar-calibrated indigo `#1a0040`; keep every other built-in profile explicitly cyan `#0096ff`. Imported theme JSON exposes the in-progress status strip / light-bar color as `led.printing`.
Pass the configured printing status color into AC-controller progress animations. Do not replace the selected palette with a hardcoded cyan or blue progress color in the puppy LED driver.
Keep xBuddy-extension RGBW PWM on its dedicated approximately 23.4 kHz TIM2 carrier and preserve raw `0-255` compare values. Do not alter TIM3 fan and chamber-light timing when tuning RGBW output. Preserve upstream `M150` override semantics: custom status animations remain active until the printer state changes, and solid `M150 A0` calibration commands bypass the normal status-animation cross-fade so hardware measurements are stable immediately. In `FrameAnimation::render()`, convert solid-frame percentages to a `0.0f` through `1.0f` factor before calling `ColorRGBW::fade()`: passing `100` directly wraps RGBW channels modulo 256 and corrupts intermediate colors.
After filtering ends, or immediately after a print that does not need filtering, unacknowledged status LEDs hold solid green for the configurable status-finished-hold duration before entering the normal idle sequence.
After the finished hold, status LED animation remains controlled by print state while brightness follows the side-light active, idle, and deep-idle state again. Screen interaction and Core One door activity wake brightness normally; an open Core One door holds active brightness until it closes.
The status-finished-hold duration defaults to 300 seconds, is exposed in Lights Settings, exports through `M154.7 H<seconds>`, and accepts `H0` to disable the solid-green hold.
The persistent print-finished screen does not count as guided activity during post-print filtering; chamber/side lighting and the LCD resume their normal idle and deep-idle timeouts.
Per-state pages expose Deep Idle, Idle, Active, Printing.
Timing settings live one level above per-state settings where users compare state entry and exit timing.
GPIO light bar state control remains independent of chamber/side LEDs.
During an active print, the temporary chamber/side light override is the only intentional coupling between chamber/side LEDs and the external GPIO light bar: override 0% forces the external light off for that print, while any non-zero override lets the external light follow its print-state enable setting.
External GPIO pins reserved for the light bar are protected from generic GPIO reconfiguration.
External light output is latched and off-debounced so transient firmware state gaps do not command off/on flicker at print start or finish.
Active-sink external light pins float before their output latch changes when turning off, avoiding visible pulses.
M150 compatibility and M151/M152/M153 behavior remain consistent across Core One, XL, MK, and MINI feature flags.
Machine-specific LED/UI code stays behind feature flags: do not instantiate status LED color screens on targets without `HAS_LEDS`, do not compile side-strip driver code where `HAS_SIDE_LEDS` is disabled, and keep XL-only enclosure second-driver handling under XL-only preprocessor guards.
Per-state LED settings pages share one runtime menu container instead of four duplicated template menu instantiations. Preserve this shape unless a replacement is proven to fit XL flash.
```

### Core One Plus And Chamber Vents

Keep printer type selection and suppress manual open-vent prompts where needed.

Important areas:

```text
src/gui/screen_menu*
include/common/extended_printer_type.hpp
src/common/config_store/
src/persistent_stores/store_instances/config_store/
src/marlin_stubs/feature/automatic_chamber_vents/
src/gui/include_COREONE/selftest_snake_config.cpp
src/gui/include_COREONEL/selftest_snake_config.cpp
```

Behavior to verify:

```text
Printer type setting offers Core One and Core One Plus.
Core One Plus is a distinct logical model that reports COREONE+ while sharing the Core One firmware compatibility group.
Core One L builds report COREONEL and do not expose the Core One / Core One Plus selector.
Core One normal file prints keep user-facing open/close vent prompts.
Core One serial prints assume the vent is already positioned correctly and do not show filament-derived prompts.
Core One Plus normal file prints use automatic vent movement unless the scanned start G-code contains an explicit M870 vent command.
Core One Plus serial prints do not infer vent position from filament; streamed M870 commands are authoritative.
Explicit M870 commands remain usable regardless of the automatic decision policy.
Core One and Core One Plus selftest/setup labels remain correct.
```

### Odometer Persistence And Manual Motion

Keep odometer persistence out of the nested server `cycle()` path. `cycle()` also runs from nested `idle()` calls while blocking G-codes such as `G123` are planning manual movement. Print finalization may still force an immediate save.

When extending `MarlinSettings::load()`, preserve the upstream startup sequence: call `reset()` before overlaying values loaded from Buddy config-store. `reset()` initializes motion defaults, planner positioning, global endstop defaults, and stepper drivers. Skipping it caused cold-boot manual Z movement to drive incorrectly and reboot the printer when Z was the first moved axis.

### Chamber Fan, Filtration, And Safety Timer

Keep the chamber fan/filtering behavior and heater safety timer changes.

Preserve filtration lifetime accounting on every stop path. Active filter usage is journaled periodically while the fan runs, and the remaining whole-second interval must be committed before a manual early stop or backend disable clears the output state.
Keep `M154.8` as the G-code trigger for the configured post-print filtration cycle and `M154.8 S0` as the stop command.

Important areas:

```text
src/common/feature/chamber/
src/common/feature/chamber_filtration/
src/common/feature/safety_timer/
src/gui/menu_item/specific/menu_items_chamber_filtration.*
src/gui/screen/screen_chamber_filtration.hpp
lib/Marlin/Marlin/src/gcode/control/M86.cpp
lib/Marlin/Marlin/src/module/temperature.cpp
src/common/feature/print_status_message/
```

Behavior to verify:

```text
Hotend and bed-heater timeouts are grouped under Settings -> Heater Safety.
Hotend timeout is independently configurable through UI and M86 S.
Bed-heater timeout is independently configurable through UI and M86 B.
Both timeouts are capped to 3600 seconds / 60 minutes.
M86 B0 disables the bed-heater timeout.
Paused/active prints keep bed safety timer reset.
Filtering can continue after print finish.
Filtering status LED indication remains independent from chamber/side LEDs.
Chamber fan/filter settings display and reset correctly.
Waiting-for-chamber and waiting-for-bed status messages still report progress.
```

### PID Settings And Autotune Persistence

Keep the user-facing PID management screen and the Marlin settings-store bridge for PID save/load.

Important areas:

```text
src/gui/screen_menu_pid.*
src/gui/screen_menu_selftest_snake.*
src/gui/screen_menu_heater_safety.*
src/gui/screen_menu_settings.*
src/gui/MItem_menus.*
src/gui/MItem_tools.*
src/gui/CMakeLists.txt
src/common/pid_autotune_status.*
src/common/CMakeLists.txt
lib/Marlin/Marlin/src/module/configuration_store.*
lib/Marlin/Marlin/src/module/temperature.cpp
src/persistent_stores/store_instances/config_store/store_c_api.*
src/common/config_store/
src/persistent_stores/store_instances/config_store/
lib/Marlin/Marlin/src/gcode/temp/M303.cpp
lib/Marlin/Marlin/src/gcode/config/M301.cpp
lib/Marlin/Marlin/src/gcode/config/M500-M504.cpp
```

Behavior to verify:

```text
Settings contains PID Settings next to Input Shaper and Phase Stepping.
PID Settings contains separate Hotend and Heatbed submenus.
Hotend PID P/I/D values are visible, editable, applied live, and stored persistently in the Hotend submenu.
Heatbed PID P/I/D values are visible in the Heatbed submenu only where PIDTEMPBED is enabled.
Hotend PID can be reset independently to DEFAULT_Kp/DEFAULT_Ki/DEFAULT_Kd.
Heatbed PID can be reset independently to DEFAULT_bedKp/DEFAULT_bedKi/DEFAULT_bedKd.
XL applies edited hotend PID values to all hotends and calls thermalManager.updatePID() so Dwarves receive updated PID values.
PID Settings exposes heater-specific autotune actions where supported.
The PID autotune screen shows heating/cooling state, temperature, cycle progress, and the new P/I/D values.
Completed UI autotunes prompt the user to save or discard the new values; save applies and persists them, discard leaves existing PID settings unchanged.
Failed UI autotunes show failure and do not offer to save incomplete values.
M303 ... U1 applies autotuned values to the live heater PID settings.
M500 persists the live PID values into Buddy config-store.
Boot settings load and M501 first run the upstream `reset()` initialization path, then restore persisted PID values and call the normal PID postprocess/update path.
Final builds must keep full M503 reporting available, including human-readable headings/comments and TMC settings. Keep this independent from `DEVELOPMENT_ITEMS` so release builds can retain the complete report without enabling development-only UI and commands.
```

### UI Theme, Assets, And Display Framework

The RME patch range includes a broad theme/resource refresh. Treat this as a first-class feature, not incidental churn.

Important areas:

```text
src/common/ui_theme.*
doc/theme-json.md
doc/examples/theme.json
color_converter.py
src/gui/screen_theme_filebrowser.*
src/gui/res/png/
src/gui/res/png_brass/
src/gui/res/png_before_resize/
src/gui/res/fnt_src/
src/resources/QoiGenerator.cmake
src/gui/qoi_decoder.*
src/guiapi/include/display.hpp
src/guiapi/include/display_helper.h
src/guiapi/include/ili9488.hpp
src/guiapi/include/st7789v.hpp
src/guiapi/include/window_icon.hpp
src/guiapi/src/display_ex.cpp
src/guiapi/src/display_helper.cpp
src/guiapi/src/gui.cpp
src/guiapi/src/ili9488.cpp
src/guiapi/src/st7789v.cpp
src/guiapi/src/window_icon.cpp
src/guiapi/src/window_progress.cpp
```

Behavior to verify:

```text
Default RME color theme is applied after boot.
Theme JSON import and validation still work.
Theme example documentation matches accepted schema.
Brass/dark/light icon assets regenerate correctly.
QOI decoder still handles generated resources.
Header/footer/icon rendering works on MINI and large displays.
Progress bars render with updated theme colors.
```

### UI Locking, Input, And Screen Polish

Keep UI lock support and user-interface polish that is outside the serial/LED feature groups.

Important areas:

```text
src/common/printer_lock.*
src/gui/screen_menu_lock_settings.*
src/gui/ScreenHandler.cpp
src/gui/ScreenPrintingModel.*
src/gui/dialogs/dialog_text_input.*
src/gui/footer/
src/gui/window_header.*
src/gui/screen_home.*
src/gui/screen_printing.*
src/gui/screen_error.cpp
src/gui/screen_menu_user_interface.hpp
src/gui/screen_menu_settings.hpp
src/gui/screen_menu_languages.cpp
src/gui/menu_item/menu_item_select_menu.cpp
src/gui/numeric_input_config_common.cpp
src/guiapi/include/numeric_input_config_common.hpp
src/guiapi/src/WindowMenuSpin.cpp
src/guiapi/src/WindowMenuSwitch.cpp
src/guiapi/src/i_window_menu_item.cpp
src/guiapi/src/window_msgbox.cpp
src/guiapi/src/term.cpp
```

Behavior to verify:

```text
Lock settings are visible and persistent.
Locked printers block print actions until unlocked.
Serial and normal print screens update buttons correctly while locked.
Text input and numeric input still handle PIN/settings use cases.
Footer heater, fan, filament, and language displays remain readable.
Home/printing/error screens do not regress on MINI or large display layouts.
```

### Hardware, ADC, And Platform Glue

Several small platform changes support the larger RME features.

Important areas:

```text
src/common/adc.hpp
src/buddy/door_sensor.cpp
src/buddy/main.cpp
src/common/utils/color.hpp
include/common/extended_printer_type.hpp
src/device/stm32f4/hal_msp.cpp
src/device/stm32f4/peripherals.cpp
lib/Marlin/Marlin/src/feature/twibus.cpp
```

Behavior to verify:

```text
Door state is available for Core One post-print/filter acknowledgement.
Extended printer type persists and is read early enough for UI and vent behavior.
Color conversion helpers remain compatible with LED/theme code.
ADC and peripheral changes do not break platform startup.
TWIBUS changes do not break xBuddy extension/light-bar support.
```

### Build And Release Tooling

Keep build progress reporting, BBF staging, local signing, and size optimization.

Important areas:

```text
build.py
utils/build.py
utils/bootstrap.py
utils/pack_fw.py
CMakeLists.txt
cmake/
cmake/ProjectVersion.cmake
cmake/Utilities.cmake
doc/firmware-signing.md
.gitignore
README.md
```

Behavior to verify:

```text
Completed parallel builds do not remain displayed as running at 100%.
BBF staging lists every generated BBF.
FIRMWARE_SIGNING_KEY can sign builds.
The machine-local default signing key path still works when present.
Release LTO link optimization uses size-oriented flags.
XL still fits boot flash.
Version metadata is correct in generated firmware.
Packaging still handles signed and unsigned BBFs.
Ignored local build/signing artifacts stay ignored.
Unused machine-specific UI and LED driver code is pruned at compile time, especially for MINI, MK3.5, Core One/Core One Plus, and XL.
```

## Conflict Hotspots

Expect conflicts or behavior drift in these areas:

```text
src/common/marlin_server.cpp
src/common/marlin_vars.hpp
src/common/serial_printing.*
src/common/feature/safety_timer/*
src/common/feature/chamber*
src/common/feature/xbuddy_extension/*
src/persistent_stores/store_instances/config_store/*
src/gui/screen_printing*.*
src/gui/screen_messages.cpp
src/gui/dialogs/DialogHandler.cpp
src/gui/ScreenPrintingModel.*
src/gui/ScreenHandler.cpp
src/gui/MItem_tools.*
src/gui/MItem_menus.*
src/gui/res/*
src/guiapi/*
src/gui/screen_menu_tune*
src/gui/screen_menu_lights*
src/gui/screen_menu_led_colors.*
src/gui/screen_menu_pid.*
src/gui/screen/screen_menu_leds.*
src/gui/screen/screen_menu_led_state.*
src/leds/*
include/leds/*
src/marlin_stubs/pause/*
src/marlin_stubs/feature/automatic_chamber_vents/*
src/marlin_stubs/M150.cpp
src/marlin_stubs/M262-M268.cpp
src/marlin_stubs/gcode.cpp
src/leds/external_light_bar.cpp
lib/Marlin/Marlin/src/gcode/control/M86.cpp
lib/Marlin/Marlin/src/gcode/queue.cpp
lib/Marlin/Marlin/src/module/temperature.cpp
lib/Marlin/Marlin/src/module/configuration_store.*
CMakeLists.txt
build.py
utils/bootstrap.py
utils/build.py
```

When upstream changes the print FSM or dialog stack, re-check all serial print state transitions and any code that reads `marlin_vars().peek_fsm_states(...)`.

When upstream changes LED APIs, re-check these separation rules:

```text
Status LEDs are independent from chamber/side LED enable state.
Temporary print brightness is independent from persistent settings.
Temporary print screen brightness is independent from persistent screen brightness settings and resets after the print.
Screen brightness is independent from chamber/side/status LED brightness.
Active and Printing screen brightness cannot be configured below 15%; the UI range must not expose Off/0% for those states.
Idle and Deep Idle screen brightness can be configured to 0%.
Zero screen brightness should write all black pixels before disabling/no-brightness-control on every supported display. ST7789/MINI should enter sleep-in. ILI9488/XLCD should force the front/status LED strip dark, then use the no-brightness-control command and display-off.
Display-driver pixel writes should be suppressed while screen brightness is zero, and non-zero screen brightness should explicitly re-enable display writes, brightness control, and display-on before setting brightness.
The transition from zero to non-zero screen brightness should invalidate the full current screen so wake redraws immediately.
Dim idle/deep-idle wake input is consumed before normal UI action dispatch.
OOB setup, calibration, self-tests, MMU tests/actions, and other visible FSM-guided flows are active use and should hold active screen/lighting behavior.
Abort indication is not treated as finished indication.
Door/filter acknowledgement is Core One-specific unless explicitly handled for another platform.
Acknowledged Core One filtration forces status LED output fully black until filtering ends, even when idle status LEDs are configured non-zero.
Machine-specific paths are compile-time-pruned where possible: Core One status-bar-off path, XL side-strip/enclosure second LED driver, and status LED color UI on targets without status LEDs.
```

When upstream changes resources, rerun or verify the resource generation path before accepting binary conflicts. The RME range intentionally changes normal, brass, and source PNG assets plus QOI generation support.

When adding shared code to external-light or LED paths, build MK3.5 as well as Core One and XL. MK3.5 has different include paths and can catch missing direct includes, such as `timing.h` for `ticks_ms()`, even when Core One and XL compile.

## Flash And Size Budget

XL is the first target to check after every feature conflict resolution. Keep the release link step size-optimized. If XL overflows:

```text
Prefer moving repeated status strings behind existing translation/string tables.
Avoid adding new large GUI assets.
Prefer shared helper functions only when they reduce generated code.
Check whether new upstream code already implements an RME behavior before keeping duplicate compatibility code.
Remove obsolete compatibility shims after verifying behavior.
Move target-specific UI, LED, chamber, enclosure, and GPIO behavior behind existing `HAS_*`, `PRINTER_IS_*`, and board feature flags before removing features.
Build at least XL, Core One, MINI, and MK3.5 after pruning. MINI catches disabled-feature UI leakage; MK3.5 catches non-side-LED status LED paths; Core One and XL catch the chamber/side LED paths.
```

Do not trade away serial-print recovery, lighting safety, or post-print acknowledgement behavior just to save a small amount of flash. First look for duplicate code, strings, and upstream replacements.

For per-state LED/screen settings, prefer the shared runtime-backed state menu container in `src/gui/screen/screen_menu_led_state.*`. Reintroducing separate `ScreenMenu<...>` template instantiations for each light state has previously pushed XL over boot flash.

Release builds should disable `DEVELOPMENT_ITEMS` by default. The local `utils/build.py --final` path keeps both version suffixes set to `-RME` and injects `-DDEVELOPMENT_ITEMS_ENABLED:BOOL=NO` unless the caller explicitly overrides it. Verify the installed firmware home-screen badge reports `<version>-RME`. Verify Prusa Connect reports the base upstream version, without the `-RME` suffix, for cloud compatibility. The stock bootloader firmware-selection screen is expected to append the mandatory BBF header build number after the `RME` tag.

Core One, Core One L, XL, and MINI store translations in the resource image to preserve boot flash headroom. Keep `COREONE`, `COREONEL`, `XL`, and `MINI` in `PRINTERS_WITH_EXTFLASH_TRANSLATIONS`. Keep the Core One/Core One L/XL `resources-image` block count large enough for ESP assets, puppy firmware, web assets, QOI data, and translation `.mo` files. If resource generation fails with `LFS_ERR_NOSPC`, increase the resource image size rather than moving translations back into CPU flash.

The latest checked focused final builds used:

```text
python3 utils/build.py --preset xl --bootloader yes --final
FLASH: 1291244 B / 1919 KB, 65.71%

python3 utils/build.py --preset mini --bootloader yes --final
FLASH: 893296 B / 895 KB, 97.47%

python3 utils/build.py --preset coreone --bootloader yes --version-suffix=-RME --version-suffix-short=-RME -DDEVELOPMENT_ITEMS_ENABLED:BOOL=NO
FLASH: 1290800 B / 1919 KB, 65.69%

python3 utils/build.py --preset coreonel --bootloader yes --version-suffix=-RME --version-suffix-short=-RME -DDEVELOPMENT_ITEMS_ENABLED:BOOL=NO
FLASH: 1290944 B / 1919 KB, 65.69%

python3 utils/build.py --preset mk3.5 --bootloader yes --final
FLASH: 1850136 B / 1919 KB, 94.15%

python3 utils/build.py --preset mk4 --bootloader yes --final
FLASH: 1919312 B / 1919 KB, 97.67%
```

Final/non-development builds intentionally keep the full `M503` settings report command enabled. `FULL_M503_REPORT_ENABLED` remains enabled independently from `DEVELOPMENT_ITEMS_ENABLED`, preserving human-readable headings/comments and TMC settings without enabling development-only UI and commands. Do not use `-fno-threadsafe-statics` as a flash fix; this firmware runs multiple tasks, and removing thread-safe function-local static initialization can race if two tasks first-touch the same local static. Prefer target-specific feature flags, duplicate-string reductions, and shared UI containers.

PID edit/autotune/save/load support must remain available through `M301`, `M303`, `M500`, and `M501`. Keep the PID Settings entry next to Input Shaper and Phase Stepping in Settings. The PID parent screen must stay split into Hotend and Heatbed submenus so reset/autotune actions are heater-specific.

Loaded filament assignments must remain visible and editable from the Filament menu without forcing a load/unload cycle. Keep **Filament -> Loaded Filament(s)** available on single-tool, toolchanger, and MMU filament menus. Material and color are persisted per virtual tool/MMU slot. The color picker includes the built-in palette and eight persistent custom slots; custom definitions must remain part of RME settings export/import. Successful load/change flows copy their requested color into the loaded-slot record, while unloading clears it.

Host-side filament discovery must remain available with:

```text
M865 Q
```

The response is one `loaded_filament T<n> S"<material>" O"<color name>" H"<#RRGGBB>"` line per enabled tool or filament slot. Preserve `M865 S"<material>" L<n> O"<#RRGGBB>"` assignment and `M865 V<0..7> N"<name>" O"<#RRGGBB>"` custom palette management.

RME fleet configuration must remain available by G-code and by export:

```text
M154.0 screen brightness by state
M154.1 chamber/side brightness by state
M154.2 status LED brightness by state
M86 S/B hotend and bed-heater safety timeouts
M154.3 lighting timeouts and door/post-print flags
M154.4 serial printing UI settings
M154.5 external light-bar state enables
M154.6 Core One / Core One Plus extended printer type
M154.7 status LED finished-hold seconds
M154.8 filtration cycle start/stop
Settings -> Export RME Settings writes /usb/rme_settings.gcode
```

## Manual Smoke Test Matrix

Minimum hardware or simulator checks before publishing:

```text
Core One / Core One Plus:
  Serial print start, pause, resume, cancel, finish.
  Manual open-vent prompt suppression for serial print and Core One Plus.
  Chamber LEDs off while status LEDs still work.
  Per-print chamber/status brightness at 0%, low value, and 100%.
  Per-state screen brightness: Active/Printing below 15% is clamped, Idle/Deep Idle can be Off, and idle/deep-idle below 15% consumes first input as wake-only.
  Door open/close acknowledgement after finished, aborted, and filtering states.
  Door-open prompt during filtering: No continues filtration, Yes ends it early, and natural filter completion closes the prompt back to the finished screen.
  Finished-screen `Stop Filter` action ends filtration early, leaves the finished summary open, and disappears when filtration ends.
  After door acknowledgement during filtering, status LEDs remain fully off even when idle status brightness/color is configured non-zero.
  External chamber light does not flicker off/on at print start or print finish.
  Heater Safety menu: hotend timeout UI/M86 S, bed timeout UI/M86 B, and 60-minute upper clamp.
  PID Settings screen: edit hotend/heatbed values, reset each heater to defaults, run autotune, confirm progress/new-value prompt, save/discard behavior, and reboot persistence after save.
  M303 autotune with U1 followed by M500 persists after reboot.
  Theme import, lock settings, and text/PIN input.
  External GPIO light bar configuration on supported hardware.

XL:
  Serial print start, pause, resume, cancel, finish.
  MMU/runout recovery sends serial host resume.
  MMU extra purge shows status progress instead of staying at 0%.
  Per-print side-strip brightness at 0%, low value, and 100%.
  Per-state screen, status, and side-strip brightness settings remain visible and independent.
  Idle/deep-idle screen brightness can be Off and wakes without activating the touched/focused UI control.
  Chamber fan/filter controls and filtering LED indication.
  PID Settings screen: hotend values are visible/editable, heatbed PID is hidden if PIDTEMPBED is not enabled, edited/autotuned values propagate to Dwarves.
  PID autotune screen shows progress/new values and prompts save/discard before persistence.
  M303 autotune with U1 followed by M500 persists after reboot.
  Theme assets and brass/dark/light icon rendering.
  Release boot image fits flash.
  XL retains the same OctoPrint SD/USB storage and per-print lighting features as the other supported printers.
  Release builds use size-oriented compile/link flags; re-check `CMakeLists.txt` before dropping `-Oz` or constant merging.

MINI:
  Serial print UI mode dropdown.
  Messages Only screen layout.
  Progress screen layout.
  Host message filtering and numeric percentage on the message page.
  Screen memory remains within `ScreenFactory` storage.
  Only screen brightness settings are shown in Lights Settings; no status/chamber/side LED controls are instantiated.
  Idle screen brightness can be Off and consumes first encoder input as wake-only.
  Theme resources render correctly on the small display.
  PID Settings screen compiles and shows only heater PID controls and autotune actions supported by the target.
  Status LED color settings and other status-LED-only code are not instantiated when `HAS_LEDS` is disabled.

MK4 / MK3.5:
  Serial print screen and post-print Continue acknowledgement.
  Abort indication does not look like finished indication.
  Status LEDs acknowledge finished/aborted states correctly on non-door platforms.
  Screen and status LED brightness controls are visible, but side/chamber controls are hidden unless the target actually supports them.
  LED manager builds and runs the non-side-LED wake path.
  Serial printing and theme changes compile under MK feature flags.
  PID Settings screen shows supported heater PID controls, autotune actions, and reset actions.
  External GPIO light bar configuration is visible and usable on supported xBuddy GPIO breakout / IO expander hardware.
  Shared external-light code compiles under MK3.5 include paths.
  LED manager builds the non-side-LED path without pulling in Core One or XL side-strip-only code.
```

## Release Notes Checklist

Create a new release notes file for the upstream version:

```text
RELEASE_NOTES_vX.Y.Z-RME.md
```

Include:

```text
Upstream tag and commit.
RME branch name.
Supported printer list.
New upstream highlights if relevant.
RME feature summary.
Serial printing changes.
LED/chamber/GPIO changes.
Theme, asset, and UI lock changes.
Safety timer and chamber fan/filtering changes.
PID settings UI and PID autotune persistence behavior.
Core One Plus and vent behavior.
Build/signing notes.
Stock Prusa bootloader limitation and expected non-genuine firmware warning.
Stock bootloader firmware-selection menus derive their displayed version from the mandatory BBF header build number; removing that numeric suffix requires a bootloader change.
Known limitations.
Focused build results for XL/Core One/MINI.
Focused build results for MK4 or MK3.5 when shared LED, GUI, display brightness, or platform guards change.
Full build summary and BBF list.
```

## Publish Checklist

Before publishing BBFs:

```text
git status --short
git diff --stat
git diff --check
./build.py --preset xl --bootloader yes
./build.py --preset coreone --bootloader yes
./build.py --preset coreonel --bootloader yes
./build.py --preset mini --bootloader yes
./build.py --preset mk4 --bootloader yes
./build.py
python3 utils/build.py --bootloader yes
```

Confirm `bbf/` contains all expected BBFs and that the build summary reports zero failures.

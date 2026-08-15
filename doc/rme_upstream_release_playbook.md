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

Keep pressure-monitor suspension reference-counted. PA batches, generic filament load/unload, and MMU command guards overlap during calibration and tool changes; monitoring must remain disabled until the outermost operation finishes. On final release, discard the pre-maintenance E/time baseline, refresh the idle baseline after settled layer travel, require three seconds and 3 mm of continuous forward extrusion before pressure evidence qualifies, then require both five continuous seconds and 5 mm of missing/collapsed pressure before raising a fault. A healthy-pressure sample, pause, retraction, load, or unload resets that bad-evidence timer and distance. Sustained high pressure alone is a soft max-flow marker; it becomes a `flow_breakout` fault only after pressure subsequently collapses. Runtime expected pressure must use the calibrated load difference, not the absolute tared low-speed load. Regression-test that thick purge lines, layer transitions, and final MMU unload cannot raise `M1601`, that every PA-related MMU unload is followed by front-strip nozzle cleaning before any cross-bed move, that the nozzle parks clear of the anchor before target restoration/cooldown, and that results below 0.75 confidence retry before completing successfully with the fallback after the bounded safety limit. A weak result must not use `SERIAL_ERROR_MSG`, because serial hosts interpret it as a print-cancel condition. Stuck-filament recovery must expose Continue, Unload, and Abort, acknowledge/rearm the fault latch, restore any displaced X/Y/Z axis before successful resume, and never emit a host resume after abort. Preserve direct paused-host actions `M1601 C`, `M1601 U`, and `M1601 A`; these must be consumed from serial RX while the foreground M1601 is blocked and ignored safely when no matching prompt is active. Aborting from the stuck-filament dialog must retain the printing FSM through normal cleanup and show the stopped-result screen. Connect may report Finished/Stopped only while the matching normal or serial result UI is active; once the home screen is active it must report Idle.

Keep two serial recovery slots outside the normal `BUFSIZE` streaming limit and
leave them active at all times, including blocking heater waits before a
pause/error FSM exists. The first absorbs a line already in flight and the
second receives the service action. Keep advanced-OK `B` reporting
clamped to the normal queue size so hosts never treat the reserve as ordinary
streaming capacity. Consume serial `M601` directly, cancel an active heater
wait before requesting the pause, and consume `M602` directly only from a
stable pause with no actionable non-printing FSM on top. Consume `M876 S<n>`,
`M876 A"<name>"`, and `M876 Q` directly from serial RX. Validate indexes and
case-insensitive canonical `Response` names against the active top-most dialog;
report normal/reserve occupancy, print state, stable-pause and safe-resume
flags, blocking command, and available canonical action names. Preserve the
direct paths for `M1601 C/U/A`, print abort, heat-wait cancel, emergency stop,
and quick-stop. Ordinary motion, extrusion, heater, and print commands retain
normal FIFO ordering. Advertise `PRIORITY_COMMAND_QUEUE`, `DIALOG_RESPONSE`,
`NAMED_DIALOG_RESPONSE`, and `SERVICE_QUEUE_STATUS` through `M115`.

Preserve the separate `@RME` plugin-control receiver. It must run after normal
line-number/checksum validation but before insertion into the G-code FIFO, and
must therefore remain responsive while a foreground heater, probe, MMU, or
calibration G-code blocks. Never register these operations as M-codes. Remote
GUI events must cross the bounded atomic mailbox and execute only in the GUI
task. Require a volatile `UI ENABLE 1` session, clear pending events and disable
the session whenever the UI locks, and re-check lock state in the GUI consumer.
Locking suppresses input only: printing, calibration, error, and filament FSM
screen updates must continue normally. Re-test query/set coverage for themes,
persistent and temporary screen/chamber/status brightness, lock configuration,
and all eight synchronized host filament presets. Confirm `M115` advertises
`RME_OOB_CONTROL`, and keep `doc/rme_serial_remote_protocol.md` synchronized
with every wire-format change. Verify `MACHINE QUERY` on single-nozzle, MMU,
XL toolchanger, and INDX builds: physical/logical tool counts and topology flags
must match the machine, XYZ/bed bounds must be reachable, and feedrate and
XYZ feedrate values must reflect the active planner settings.

Keep PA service travel collision-safe. CORE One/Core One L front-edge anchors begin to the right of the vent lever and enter deep-front Y through an ordered safe-Y/X/Y path. After the local probe finishes inside that safe corridor, move directly to the off-bed extrusion point; do not route through generic park and then reverse direction. The 10 mm heating clearance is an idempotent absolute minimum, and the 170 C preheat must not start until homing is complete and that clearance exists. INDX must continue using `mapi::park(ParkPosition::purge)` rather than direct XY motion so it exits the dock area perpendicularly and applies the calibrated nozzle-cleaner/waste-bin avoidance pattern.

For serial reliability, keep M976 on the standard Marlin keepalive cadence and preserve the PA-active receive-path bypass for cosmetic numbered `M117`/`M73` updates. OctoPrint status plugins can inject these before the long-running M976 receives its final `ok`; queueing them exhausts Marlin's small command ring, stops USB RX draining, and causes real checksum/line-number resends. The bypass must parse host progress, acknowledge the numbered line, and discard only these cosmetic commands. Do not bypass motion, temperature, pause, cancel, or safety commands.

Preserve the PA noise-floor and search contract: capture 300 ms while stationary,
use derivative-free golden-section refinement within the fallback safety bracket,
then measure the selected point and both 0.002 neighbours regardless of whether
the acceptance floors have already been reached. Start at the firmware material
preset, probe both directions with a 0.020 step, then shrink the selected
bracket to the 0.002 verification scale. Preserve all three persistent controls
in the UI, G-code, and RME settings export/import:
`pa_confidence_floor_percent` (75, 50–95, `Q`), `pa_minimum_snr` (6.0, 3–20,
`N`), and `pa_confidence_retries` (6, 0–10, `R`).

Verify the root Filament menu is the only loaded-filament entry point. Each
loaded tool row must show material and color name with an outlined visual swatch, its editor must stage
Material and Color independently and commit only on Save, and the color chooser
must omit unused user slots. On non-MINI printers, verify Settings > Filament Colors creates a named
color through the color-value picker and that `M865` retains the exact hex value
for serial hosts while the loaded-filament list does not display preset metadata.
Every interactive load, change, MMU preload, and preload-all path must request
both material and color. Verify the selected pair is committed to the physical
tool or MMU slot only after a successful operation. Every successful unload,
including the confirmed-empty fast path, must clear both fields.
Every color choice must render the same theme-safe bordered swatch used by the
Loaded Filaments page. Verify built-in and populated user colors appear in both
normal-load and MMU-preload selectors, the selected preload row no longer reads
`Don't change`, and a newly created user color remains visible with its preview.

Verify **Filament > Loading test** lists each enabled MMU slot and one **Test
All** action. Exercise `M1704 P<n> M0` for one physical slot, `M1704 K<mask> M0`
for an arbitrary set, and `M1704 A M0` for all enabled slots. Multi-slot tests
must execute sequentially and retain the normal MMU error and user-abort paths.

Verify Settings > FW update opens a page containing Select BBF from USB and
Update Instructions. Its browser must list `.bbf` files in the USB root and
subdirectories while excluding ordinary print files; the normal print browser's
G-code-only filter must not hide firmware. The picker must reject non-BBF files and paths outside
`/usb`, but accept completed Connect downloads in USB subdirectories and long
filenames. After confirmation it must copy the selection to the neutral
`/usb/FWUPD.RME` staging name, flush it, and create the marker plus retained
bootloader selection only in the M997 reboot handoff. An incomplete or
completed-but-unselected stage must never use a `.BBF` extension and must not
flash after an unrelated reboot. A `/usb/FWUPD.UI` marker causes the
application to remove the temporary stage after the requested attempt. Test
copy failures, invalid-signature, and wrong-model
files to ensure the bootloader rejects them without flashing. Serial regression
coverage must retain `M997 O` and `M997 /usb/FIRMWARE.BBF`; the latter selects a
file already present on USB rather than carrying the file data itself.
The copy buffer must remain outside the GUI task stack, and the picker must
queue `M997 /usb/FWUPD.RME` rather than resetting directly inside its click
callback. Confirm that the UI remains responsive during staging and that the
Marlin-side command restarts the printer immediately afterward.

RME's binary upload layer is `M998`: verify begin rejects zero/oversized files
and malformed SHA-256, chunks reject invalid Base64 and any offset other than
the last acknowledged byte, finalization rejects size/hash mismatch, and abort
removes `/usb/FWUPD.TMP`. A successful finalization must atomically expose
`/usb/FWUPD.BBF` without rebooting; only a later `M997 /usb/FWUPD.BBF` may
request flashing. Exercise media-removal and write-failure paths as well as a
successful signed-BBF upload.

MINI must retain the same firmware-update menu and M998 validation/state
machine as MK/CORE builds. MINI and all xBuddy targets keep immutable bitmap
fonts in `/internal/res/fonts`, generated from the selected complete font
headers as independently compressed random-access glyph records in the
resources tarball. MINI packages 7x13, 9x16, and 11x18; xBuddy packages 9x16,
11x19, 13x22, and the digits-only 30x53 font. Verify Japanese, Ukrainian, and Latin MINI presets,
including resource replacement after an update, missing/corrupt-resource
behavior, repeated-glyph caching, and that no embedded font arrays return to
the MINI or xBuddy application link map. Build MK4, MK3.5, CORE One, CORE One
INDX, and CORE One L after changing this path. Keep meaningful internal-flash
headroom; do not remove update UI or language coverage to make an image link.

## Current Baseline

RME 6.8.1 is based on upstream tag `v6.8.1` at `026c2ee1f`.

The active release branch is:

```sh
rme-v6.8.1
```

The release notes for the active release are:

```sh
RELEASE_NOTES_v6.8.1-RME.md
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
git diff --stat v6.8.1..rme-v6.8.1
git diff --name-status v6.8.1..rme-v6.8.1
git diff --dirstat=files,0 v6.8.1..rme-v6.8.1
```

At the 2026-07-25 release audit, the RME 6.6.2 release port covered 648 files
with 21,102 insertions and 1,399 deletions relative to upstream `v6.6.2`; the
6.5.7 port covered 656 files with 19,933 insertions and 2,721 deletions relative
to `v6.5.7`; and `master` covered 685 files with 21,450 insertions and 1,581
deletions relative to `upstream/master`. The audited heads before the release
documentation commits were `37d8ebd89`, `a1b9e64de`, and `bc492d13f`
respectively. The stack contains the complete serial-printing, PA
calibration, filament metadata, USB/serial firmware update, INDX, lighting,
build-tooling, safety/chamber, PID, Prusa Connect, and GUI feature set. Recompute
these figures whenever the release branch advances; they are an audit aid, not
a release invariant.

The original 6.5.3 source patch range remains useful for archaeology and conflict comparisons:

```sh
coreone-v6.5.3-patches
git diff 3fc7b43a3b9bd77e9267647af5d96fe1ee7cb1c2..coreone-v6.5.3-patches
```

## Supported Release Targets

Build these presets for a full release:

```sh
coreone
coreone_indx (6.6.2 and newer)
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
git log --oneline v6.6.3..rme-v6.6.3
git cherry-pick <first-rme-commit>^..<last-rme-commit>
```

If the old branch has extra experimental commits, cherry-pick the feature groups below instead of the whole range.

3. Before resolving conflicts, regenerate the changed-file inventory from the old release branch and compare it to this playbook.

```sh
git diff --name-status v6.6.3..rme-v6.6.3 > /tmp/rme-files.txt
git diff --dirstat=files,0 v6.6.3..rme-v6.6.3
```

Every non-resource source directory in that inventory should map to one of the feature groups below. If a file does not fit, add a new playbook item before finishing the rebase.

4. Resolve conflicts by feature area. Do not resolve mechanically; upstream often changes GUI, FSM, and Marlin integration code between releases.

5. Run the narrow compile checks early.

```sh
./.venv/bin/python utils/build_tests.py rme_protocol_tests transfers_tests connect_tests --jobs 4
./build/tests/tests/unit/rme/rme_protocol_tests
./build/tests/tests/unit/transfers/transfers_tests
./build/tests/tests/unit/connect/connect_tests
./build.py --preset xl --bootloader yes
./build.py --preset coreone --bootloader yes
./build.py --preset coreonel --bootloader yes
./build.py --preset mini --bootloader yes
```

The RME parser suite is a release gate for service/G-code isolation, complete
32-bit synchronization transaction IDs, strict fixed-capacity decoding,
manufacturer escaping, `/usb` path confinement, SHA-256 fields, and repeated
host synchronization without retained parser state. Keep the transfer and
Connect suites beside it because those paths share the same host connection
and file-transfer lifetime constraints.

XL is the side-LED/enclosure gate. MINI is the layout and small-display/screen-only brightness gate. Core One and Core One L are the chamber/door/LED/resource-image gates. MK4 or MK3.5 is the non-side-LED status/display brightness gate.

6. Run the full release build after focused targets pass.

```sh
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

On failure, inspect `.rme-build-errors/<version>/<preset>.log`; the wrapper
stores the complete preset output there before printing its shortened summary.
Do not place these diagnostics in `bbf/`, which remains artifact-only.

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

9. Confirm `bbf/` contains only version directories and the artifacts produced
by the final command. Update README, the G-code guides, this playbook, and the
release-note commit ledger before tagging. Tag the documentation commit, push
the release branches and tags, then verify local and remote branch hashes match.

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

The 2026-07-25 audit compared `v6.5.7..rme-v6.5.7`,
`v6.6.3..rme-v6.6.3`, and `upstream/master..master` by name-status and file
dirstat. Every changed non-resource directory maps to the feature groups below;
PNG/font/resource-only directories map to UI Theme and MINI external resources,
and tests map to the feature whose production code they exercise.

### Pressure Advance, Extrusion Health, And MMU Calibration

Preserve `M976`, its batch manifest parser, material/temperature preflight,
RAM-only calibrated-value authority, fallback handling, confidence controls,
debug toggle, manual Control-menu UI, blocking FSM, and slicer templates. MMU
systems must probe unloaded, skip unload only when working FINDA and extruder
sensors both prove the path empty, and treat disabled/faulted sensors as
filament present. The PA-specific load, unload, and free-air excitation share
the same per-slot position at `Y_MIN+1`; the load extends exactly 2 mm beyond
the modeled nozzle path. Clean after a real unload before any cross-bed move,
restore previous temperatures, and leave the nozzle unloaded for the slicer's
following MBL. Anchor occupancy is RAM-only and scoped to one print job:
`reset_job_results()` must clear it so a cleared sheet is not rejected by the
next print, while `M976 C L<slot>` permits an explicit same-job retry. INDX
continues to use dock-aware purge-bin travel and counted fast/slow pellets.

Important areas include `src/marlin_stubs/M976.cpp`,
`src/common/feature/extrusion_calibration.*` (or `src/feature/` on newer
upstream), `src/gui/screen_menu_pa_calibration.*`, the PA FSM types,
`lib/Marlin/.../MMU2/mmu2_mk4.*`, pressure-advance planner code, config-store
items, G-code documentation, and slicer templates.

### Filament Material, Color, And Loading Tests

Preserve per-virtual-tool material/color persistence, built-in colors, eight
populated-only custom color slots, theme-safe swatches, `M865`, import/export,
and the single root Loaded Filaments page. Every successful interactive load,
change, or MMU preload commits both selected fields; every successful unload or
confirmed-empty fast exit clears both. Preserve `M1704` single-slot, mask, and
all-slot loading tests and their matching UI.

### USB And Serial Firmware Updates

Preserve the USB BBF picker, `/usb/FWUPD.BBF` staging, marker-based cleanup,
queued `M997` reboot handoff, Connect subdirectory/long-name support, and the
validated `M998` begin/chunk/finalize/abort upload protocol. MINI retains the
same feature-complete update path by keeping large fonts in external resources.

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
Latch each active MMU error identity after its first UI/serial report. Suppress the MMU's periodic identical error repeats until `EndReport` confirms that recovery or the operation completed; report a changed error immediately and re-arm the latch for later operations.
M75, startup M73, and OctoPrint startup messages received while a serial print is already active must not arm a pending second print start. End G-code sent after M77 must not restart serial print state or clear the frozen completed-print duration.
Ignored short serial macro prints, such as a quick M75/M77 pair with no printed Z height and less than the minimum useful duration, must restore the previous Marlin print state, active print screen, and LED state. If the previous state was a persistent finished screen that was still visible, keep that finished screen and green finished indication; if no print FSM was visible, return to idle/home behavior.
Canceling a detected serial print before its initial home must not issue an unhomed Z clearance move when no printed Z height exists. Shut down heaters and continue abort cleanup without moving the steppers.
Serial-origin M601 must stop serial intake, drain queued moves, park the head, keep the print in the active paused print lifecycle, then report `//action:paused firmware_pause`. Do not echo a fresh `//action:pause` back to the same serial host that sent M601, because OctoPrint can recursively run host pause scripts and turn heaters off.
Serial-origin M602 must resume through the firmware unpark/reheat path and report `//action:resumed` after the printer is actually continuing. Do not echo a fresh `//action:resume` back to the same serial host that sent M602.
Screen- or firmware-origin pause/resume during a serial print must still send the request action and the completed action (`pause` then `paused`, or `resume` then `resumed`) so OctoPrint runs its after-pause and after-resume scripts.
MMU/runout and other firmware-initiated manual-intervention recovery sends the host resume action and then reports resumed after the printer is actually continuing.
An ordinary successful MMU load or tool selection must not emit
`//action:resume`. The Marlin-server bridge may forward an MMU resume only when
it previously exposed the matching MMU-error pause to that serial host.
If a host retransmits a valid numbered line whose sequence number is already
accepted, acknowledge it without executing it again. Continue rejecting bad
checksums and genuinely missing forward sequence numbers.
When an MMU, side, or xBuddy-extension filament sensor is the primary print-time runout source, keep the toolhead/extruder sensor active as a secondary runout source so broken or stuck filament between the upstream sensor and extruder still triggers the normal M600 pause. Avoid duplicate runout injection by skipping the secondary check when both logical sensors resolve to the same physical sensor.
On INDX, `M591 S` and the `Loadcell Filament Runout` GUI toggle control the fast missing-pressure presence detector; `M591 U` and `Loadcell Filament Movement` independently control established-pressure collapse detection. Runout requires meaningful continuous executed extrusion and uses a short two-stage debounce, while collapse retains the conservative long debounce. The calibrated flow-breakout/max-flow safety path is always enabled and must not be gated by either setting. These causes inject `M1601 R1/R2/R3`, present distinct runout/not-moving/flow-limit guidance, and emit distinct RME workflow/code pairs `filament_runout/runout`, `filament_movement/not_moving`, and `extrusion_flow_limit/flow_limit`. Plugins must route these stable fields rather than screen text and retain the cause while the shared filament recovery FSM runs. Non-INDX loadcell machines retain the legacy E-stall semantics and labels. Preserve `M591 S/U/P/R/I/F` behavior where supported so users can enable, persist, restore defaults, and tune legacy skip debounce without requiring a rebuild.
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

For the out-of-band host protocol, verify `@RME FILE CAPS`, root listing,
stat/read, a SHA-256-verified binary upload, durable suspend/resume, framed
binary-mode controls, crash-dump export, explicit abort cleanup, rename/delete,
print queueing, and BBF flash queueing on both maintained branches. Confirm
path traversal and attempts to address non-USB storage are rejected, partial
uploads never appear under the final name, and passive RME traffic still lets
the printer enter idle.

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

All xBuddy targets also store their immutable bitmap fonts in that managed
resource image even when translations remain linked normally. Preserve the
shared single-glyph cache in `fonts.cpp`; a cache per font wastes scarce RAM.
The font payload is part of the BBF resource digest and must be installed by
the normal bootstrap/update flow before the GUI enables resource-backed drawing.

The latest checked focused final builds used:

The xBuddy external-font change was release-validated with
`./build.py --final --versions 6.5.7 6.6.3 --jobs 15`: all 29 presets passed.
The latest filesystem/profile/session build produced MK4 flash usage of 93.89%
on 6.5.7 and 59.99% on 6.6.3; all 29 presets passed. The largest MINI images
used 95.61% on 6.5.7 and 97.57% on 6.6.3.
Keep these as regression baselines; a later port that returns either MK4 to the
partition boundary has probably relinked the font arrays into application flash.

For the out-of-band protocol, preserve the separation between structured RME
events and Marlin transport/state traffic. `legacy=0` may suppress only
`//action:notification`; never suppress temperatures, `ok`, `busy`, resend,
emergency, or pause/resume/cancel actions. Validate session open/query/
keepalive/close, event masks, sequence reset on open, reconnect negotiation,
the 30-second session lease (with 10-second keepalives), automatic restoration
of legacy notifications after expiry, generic dialog action discovery, MMU
recovery, filament runout, stuck-filament
choices, and tool-change recovery. Keep workflow actions sourced from the live
FSM so a plugin cannot apply stale recovery policy.

Keep manufacturer separate from the upstream filament `base_preset`: the
former identifies the loaded spool vendor, while the latter selects inherited
material behavior. Preserve the ordered 50-entry built-in list, eight custom
slots, case-insensitive RME lookup, per-tool clearing on unload, keyboard load
and preload selection, M865/settings replay, and percent-encoded RME names.

The host link is USB CDC. Advertise 1,000,000 as the preferred normal-mode rate,
retain 250000, 230400, and 115200 as compatible fallbacks, and keep 57600 as
diagnostic logging mode; line coding does not change the USB wire clock.
Preserve the 512-byte CDC RX/TX FIFOs for multi-packet upload bursts. Retest
numbered legacy G-code, resend handling, M20-M32, M998, the 48-byte RME upload,
four-frame 384-byte bulk windows, and eight-frame 1024-byte raw binary windows.
Do not enlarge both CDC FIFOs to a complete host window: they consume ordinary
SRAM and directly reduce runtime heap headroom. Keep binary upload/download on
one shared static payload buffer, keep the legacy M998 staging stream open for
the whole transfer, and give every RME-owned stdio stream an explicit static or
unbuffered policy so newlib cannot add a hidden per-operation heap buffer.
The transfer subsystem is globally single-instance, so keep its recovery
scratch, async HTTP engine, decryptor, inline state, intrusive partial-file
ownership, and both USB DMA sector buffers in their fixed pools. Do not replace
these with `make_unique`, `make_shared`, default-buffered `FILE` streams, or
per-transfer sector allocations: connection/recovery overlaps USB, lwIP, GUI,
and host synchronization startup and must not depend on runtime heap headroom.
Likewise, `SharedBuffer::Borrow` is intentionally intrusive and copyable. Keep
Connect command/path/token/hostname lifetime management on that fixed buffer;
wrapping borrows in `shared_ptr` recreates a control-block allocation for every
host command and can fragment memory during reconnect synchronization.
For raw mode, cover reasoned CRC and offset NACK recovery, binary and ASCII
abort escapes, disconnect/reboot resume from persisted metadata, reserved
CRC-protected RME control frames, wrong-hash cleanup, atomic rename, crash-dump
export, and BBF flash handoff. Never feed raw bytes through the G-code queue or
permit unframed ASCII commands to be interleaved before raw mode completes or
aborts.

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

Loaded filament assignments must remain visible and editable from the Filament menu without forcing a load/unload cycle. Keep **Filament -> Loaded Filament(s)** available on single-tool, toolchanger, and MMU filament menus. Material and color are persisted per virtual tool/MMU slot. The color picker includes the built-in palette and eight persistent custom slots; custom definitions must remain part of RME settings export/import. Every interactive load and MMU preload prompts for material followed by color. Successful load/change flows copy both requested values into the loaded-slot record, while every successful unload clears both.

MMU load-test control must remain available from both the UI and G-code:

```text
M1704 P2 M0  test physical slot 2
M1704 K21 M0 test physical slots 0, 2, and 4
M1704 A M0   test all enabled slots
```

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
MMU host pause/resume and duplicate numbered-line recovery changes.
Filament material/color load and unload behavior.
M1704 individual, mask, and all-slot load-test behavior.
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

## 2026-08-09 build 32 / build 4 validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. All 29
presets must pass, producing exactly 14 BBFs under `bbf/6.5.7` and 15 under
`bbf/6.6.3`, with no root-level or unrelated BBFs.

Validated maxima: 6.5.7 MINI 96.52%, MK4 94.53%, MK3.5 89.97%, XL 69.12%;
6.6.3 MINI 98.47%, MK4 60.65%, MK3.5 56.02%, XL 68.79%, CORE One INDX
65.37%.

Keep immutable QOI data, translations, and supported fonts in the managed
external resource payload. Keep manufacturer presets in one packed blob and
resolve heterogeneous multi-filament rows once so runtime configuration loops
do not instantiate duplicate per-tool bodies.

INDX has eight real tools; Marlin's ninth EXTRUDERS value is `NoTool` and must
never be advertised. Dock setup offers only four- and eight-tool variants and
clears stale calibration bits above the selected count. Flashing status text
must remain below the splash progress bar in the application-owned status area
on both displays so it cannot clip the logo or reveal bootloader text.

Release tags: `v6.5.7-RME-b32` and `v6.6.3-RME-b4`.

## 2026-08-10 build 33 / build 5 validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. All 29
presets passed, producing exactly 14 BBFs under `bbf/6.5.7` and 15 under
`bbf/6.6.3`, with no root-level or unrelated BBFs.

Validated maxima: 6.5.7 MINI 96.67%, MK4 94.58%, MK3.5 90.02%, XL 69.17%;
6.6.3 MINI 98.62%, MK4 60.70%, MK3.5 56.07%, XL 68.85%, CORE One INDX
65.43%.

For staged firmware, create the durable one-shot marker before reset and
consume `FWUPD.BBF` whenever mounted media becomes available after boot, not
only on a media insertion edge. Keep the marker if unlink fails so cleanup is
retryable. Send and flush the RME reconnect marker, detach USB CDC, then reset.

Loaded-filament `RME_CHANGE` must follow all material, color, and manufacturer
writes. Exclude every `@RME` service frame from physical activity timers and
bound low-brightness display wake to 30 seconds. RME uploads may replace only
regular files and must retain the old destination until verified installation
of the new sibling succeeds.

Verify `binary_read=1 binary_read_chunk=1024`, one raw download frame per
`READ_BINARY` request, little-endian offset/length/CRC validation, retry at the
same offset after corruption, and return to line mode before the completion
record and `ok`. The RME header icon must remain the indigo R with progress for
both upload and multi-frame download ownership; legacy 48-byte Base64 reads
must remain available. Transaction IDs must round-trip over the complete
unsigned 32-bit range, including values above `2147483647`.

Release tags: `v6.5.7-RME-b33` and `v6.6.3-RME-b5`.

## 2026-08-10 build 34 / build 6 validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. All 29
presets passed, producing exactly 14 BBFs under `bbf/6.5.7` and 15 under
`bbf/6.6.3`, with no root-level or unrelated BBFs.

Validated 6.6.3 maxima: MINI 98.66%, MK4 60.76%, MK3.5 56.13%, XL 68.86%,
and CORE One INDX 65.43%.

Run `rme_protocol_tests` as a release gate on both branches. It must pass all
400,081 assertions across eight cases, including full-width transaction IDs,
strict fixed-capacity decoding, `/usb` path confinement, SHA-256 parsing,
service-frame isolation, and the 100,000-iteration retained-state stress case.
Also run the transfer and Connect suites after changes to their shared command
or buffer ownership paths.

Release tags: `v6.5.7-RME-b34` and `v6.6.3-RME-b6`.

## 2026-08-10 build 35 / build 7 validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. Require all
29 presets to pass, with exactly 14 BBFs under `bbf/6.5.7`, 15 under
`bbf/6.6.3`, and no root-level artifacts.

Connect retry and range-restart paths must reopen the live `PartialFile` in
place. Never construct a replacement while the original object remains alive:
the class deliberately owns one fixed singleton DMA sector pool and rejects a
second instance with `Concurrent partial files`. Validate the backing file's
size, contiguity, USB logical unit, and first-sector mapping before resuming.
This recovery path must not allocate heap memory and must remain independent of
simultaneous ordinary motion commands.

Release tags: `v6.5.7-RME-b35` and `v6.6.3-RME-b7`.

Final result: all 29 presets passed. The 6.5.7 set contains 14 BBFs and the
6.6.3 set contains 15 BBFs. Validated maxima were 6.5.7 MINI 96.65%, MK4
94.63%, MK3.5 90.05%, XL 69.16%; 6.6.3 MINI 98.67%, MK4 60.76%, MK3.5
56.14%, XL 68.86%, and CORE One INDX 65.44%.

## 2026-08-10 build 36 / build 8 validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. Require all
29 presets to pass, with exactly 14 BBFs under `bbf/6.5.7`, 15 under
`bbf/6.6.3`, and no root-level artifacts.

RME session reports must use `lease=<0|1>` for communications health and
`printer_state=<state>` for the real machine state. Do not reintroduce the
ambiguous session field `active`, because generic host state parsers may keep
an idle printer and its lights active. Keepalive and read-only synchronization
traffic must remain excluded from user-activity and serial-print timers.

Release tags: `v6.5.7-RME-b36` and `v6.6.3-RME-b8`.

Final result: all 29 presets passed. Validated maxima were 6.5.7 MINI 96.65%,
MK4 94.64%, MK3.5 90.06%, XL 69.17%; 6.6.3 MINI 98.68%, MK4 60.77%, MK3.5
56.14%, XL 68.86%, and CORE One INDX 65.44%. Artifact counts were exactly 14
and 15 respectively.

## 2026-08-11 build 37 / build 9 validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. Require all
29 presets to pass, with exactly 14 BBFs under `bbf/6.5.7`, 15 under
`bbf/6.6.3`, and no root-level artifacts.

RME file failures must identify offset/CRC/size/decode/storage/hash causes
instead of collapsing them into `write_failed`. A fatal raw-transfer failure
must close the stream and restore line mode. Binary abort, including the exact
ASCII compatibility escape received while malformed raw framing owns RX, must
retain the committed partial. A matching BEGIN must reopen and re-hash that
prefix, advertise its nonzero resume offset, and permit switching from binary
to bulk or legacy transport. Explicit line-mode ABORT continues to discard the
partial. Run the RME host tests for diagnostic classification and the raw-mode
ASCII abort regression before release.

Release tags: `v6.5.7-RME-b37` and `v6.6.3-RME-b9`.

Require `binary_control=1 binary_control_offset=4294967294
resumable_abort=1 durable_resume=1 crash_dump=1 shared_transfer_latch=1` in
FILE CAPS. Verify a framed
control is CRC checked, never written or hashed, and completes before upload
frames resume. Disconnect must close the stream and clear raw RX ownership
without deleting `.rme-part` or `.rme-meta`; an identical BEGIN after reconnect
or restart must derive the committed size from disk and re-hash that prefix.
Private `.rme-part`, `.rme-meta`, and `.rme-old` siblings, `FWUPD.RME`, and
`FWUPD.UI` must not appear in either FILE LIST or Connect directory reports.
Known sidecar paths must remain explicitly deletable through RME while the
protected candidate remains UNSTAGE-only. Crash-dump export must remain
allocation-free and be refused while the printer is busy.

Plugin conformance must persist path, size, and SHA-256 before BEGIN, prove an
identical BEGIN resumes a hidden partial at the committed offset after
reconnect, and prove BEGIN followed by line-mode ABORT removes its partial and
metadata as one latch-owned cleanup operation. Also test recovery after losing
host provenance: explicit STAT/DELETE of the two derived sidecar names is
allowed while idle, `not_found` is treated idempotently by the plugin, and no
directory scan or speculative `.rme-old` deletion is performed.

`RME_ENVELOPE` must report the normalized slicer-usable build volume on every
machine, never raw Marlin travel bounds. Release checks must verify representative
models with overtravel or service areas: CORE One X is 250 mm, MINI Z is 180 mm,
and XL Z is 360 mm; all minima are zero. Docking, wiping, purge, homing, and
tool-offset reach remain firmware-private motion geometry.

Run `rme_protocol_tests`; the release gate is 400,145 assertions across 15
cases before the complete firmware matrix.

Use `transfers::Monitor::instance` as the only RME/Connect/Link storage latch.
An RME BEGIN must acquire its move-only slot before touching durable metadata,
partial files, or raw RX state, report committed bytes through that slot, and
release exactly once. Only verified SHA-256 plus atomic publication is
`Finished`; abort/disconnect is `Stopped`, storage failure is `ErrorStorage`,
and all remaining failures are `ErrorOther`. While any slot exists, reject RME
reads, mutations, PRINT, FLASH, and Connect StartPrint. Reject Link/slicer HTTP
upload with Conflict while printing. A Connect download interrupted by a
print must force-save progress, close its network download, retain both slot
and partial file without consuming retries, and resume after printing.

For INDX M976, capture only fast/slow free-air excitation at the calibrated
purge parking pose over the bucket. Pause capture and disable calibration mode
before each `eject_blob` silicone-cleaner cycle, account its pellet, return to
the purge pose, synchronize, then resume capture. Unit-test that cleaner
samples are excluded while later excitation samples append to the same buffer.

Final shared-latch rebuild: all 29 presets passed. Maxima were 6.5.7 MINI
97.03%, MK4 94.81%, MK3.5 90.24%, XL 69.34%; 6.6.3 MINI 99.14%, MK4 60.96%,
MK3.5 56.33%, XL 69.04%, and CORE One INDX 65.63%. Artifact counts were 14
and 15 with no failed presets.

## Authoritative update-stage and raw-recovery release gate

Require `firmware_status=1 firmware_unstage=1 binary_timeout_ms=<n>` in FILE
CAPS. `@RME FIRMWARE QUERY` must ignore every ordinary `.BBF`, report only the
protected `FWUPD.RME` candidate with size and SHA-256, and derive `armed=1`
from the exact retained `FWUPD.RME` bootloader selection plus `FWUPD.UI`
marker. Verify that application startup clears the retained armed state and
retries protected candidate cleanup.

`@RME FIRMWARE UNSTAGE` must be idempotent, acquire the shared transfer latch,
reject printing/armed/transfer-owned states, and remove only the protected
candidate, its private `.rme-part`, `.rme-meta`, `.rme-old` siblings, and its
private marker. Generic DELETE and RENAME must not mutate that candidate.

Exercise the raw receiver with loss, duplication, CRC corruption, a declared
length of 65535, disconnect in header and payload, and abort/control frames
during recovery. The receiver must issue one recovery NACK, find the next
plausible ten-byte header without blind length-based discard, and either resume
at the committed offset or restore line mode. Verify inactivity suspension
reports the committed offset and `resumable=1`, releases the monitor slot, and
allows a matching BEGIN to re-hash and resume the durable partial.

## 2026-08-12 build 38 / build 10 parser isolation validation

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. Require all
29 presets and `rme_protocol_tests` to pass, with exactly 14 6.5.7 and 15
6.6.3 BBFs staged.

Exercise at least one complete four-command text/bulk window delivered in a
single CDC burst. Filesystem waits that service the scheduler must not
recursively enter the stateful serial line reader. Every completed RME frame
must remain immutable until dispatch returns; no Base64 suffix may reach the
ordinary G-code queue or produce `echo:Unknown command`. Verify cumulative
ACK offsets remain monotonic and the final SHA-256 matches.

Release tags: `v6.5.7-RME-b38` and `v6.6.3-RME-b10`.

The private-transfer-artifact release advances these to
`v6.5.7-RME-b39` and `v6.6.3-RME-b11`. Both Connect and RME listings hide the
protected candidate, handoff marker, partial payload, resume metadata, and
rollback sibling. Plugins retain explicit resume/cleanup access through their
durable manifest, identical BEGIN, line-mode ABORT, and derived-path
STAT/DELETE workflows.

Final result: all 29 presets passed with zero failures. Validated maxima were
6.5.7 MINI 97.35%, MK4 94.96%, MK3.5 90.40%, XL 69.53%; 6.6.3 MINI 99.42%,
MK4 61.10%, MK3.5 56.48%, XL 69.23%, and CORE One INDX 65.80%. Artifact
counts were exactly 14 and 15, with no root-level BBFs.

## Text-bulk CDC backlog regression gate

The TinyUSB CDC RX FIFO must hold every complete encoded text-bulk command
behind the command currently being committed. For the advertised 384-byte,
four-command window this requires more than 512 bytes; the configured 2048-byte
RX FIFO is compile-time checked against the shared protocol constants. Do not
reduce it or enlarge `bulk_chunk`/`bulk_window` independently. A full-speed USB
stress upload must complete without duplicated/truncated lines, `Unknown
command` payload fragments, `upload_state`, per-chunk delays, or a reduced
window. Run `rme_protocol_tests` and require the current 400,145 assertions
across 15 cases.

The CDC-backlog and usable-envelope release advances the tags to
`v6.5.7-RME-b40` and `v6.6.3-RME-b12`. The final matrix must contain exactly
14 and 15 BBFs. Validated maxima for this release are 6.5.7 MINI 97.37%, MK4
94.97%, MK3.5 90.40%, and XL 69.53%; 6.6.3 MINI 99.43%, MK4 61.09%, MK3.5
56.46%, XL 69.21%, and CORE One INDX 65.80%.

## Cause-specific extrusion workflow release gate

Build with `./build.py --final --versions 6.5.7 6.6.3 --jobs 15`. Require all
29 presets and exactly 14/15 versioned BBFs. `M1601 R1/R2/R3` must retain the
runout, movement-collapse, and flow-limit cause through the shared recovery
FSM. RME error subscribers must receive the stable workflow/code pairs
`filament_runout/runout`, `filament_movement/not_moving`, and
`extrusion_flow_limit/flow_limit`; plugins route on these fields rather than
translated text. On 6.6.3 INDX, verify the fast missing-pressure detector's
meaningful-flow and two-stage debounce guards, independent long movement
collapse policy, and always-on max-flow breakout regression.

Persistent release tags: `v6.5.7-RME` and `v6.6.3-RME`. Each firmware version
has exactly one GitHub release. For subsequent builds, force-update the
version's release tag to the validated branch tip, replace all existing BBF
assets on that release, and replace its notes in place. Never create a
per-build `-bN` release or retain superseded BBFs on the persistent release.
The combined matrix passed
29/29 presets with zero failures. Validated maxima were 6.5.7 MINI 97.38%, MK4
95.01%, MK3.5 90.41%, XL 69.58%; 6.6.3 MINI 99.44%, MK4 61.14%, MK3.5 56.47%,
XL 69.26%, and CORE One INDX 65.86%.

# 6.5.7-RME Firmware for Prusa CORE One, XL, MK4, MK3.9, MK3.5 and MINI

## Summary

  * New features and improvements
    * Serial printing screen for OctoPrint and other serial hosts
    * Persistent print-finished summaries with duration, completion time, and remaining filtration time
    * OctoPrint-compatible printer SD/USB storage commands for host-side file upload, listing, selection, print-from-SD, status, and delete
    * G-code command to trigger or stop a configured filtration cycle
    * Prusa Connect compatibility reporting so Connect sees the stock upstream firmware version while local RME branding remains visible
    * Improved serial print start and finish detection
    * Host-preferred serial print progress reporting
    * Improved serial host message filtering
    * Post-print status LED filtering indication and configurable finished hold [C1, XL, MK]
    * Slow green filtering status LED indication [C1, XL]
    * Per-state chamber/side LED brightness, timeout, and sequence behavior [C1, XL]
    * Per-state screen brightness controls for active, idle, deep idle, and printing states
    * Per-print Tune menu percent controls for chamber/side lighting, status LED brightness, and screen brightness
    * Temporary lighting overrides isolated from LCD brightness with readable wake behavior for screen-off print overrides
    * Core One / Core One Plus printer type selection [C1]
    * Serial printing UI mode selection for legacy, messages-only, and progress views
    * Per-state external GPIO light bar control for supported xBuddy GPIO breakout hardware [C1, MK]
    * Improved chamber fan and filtration controls [C1, XL]
    * Configurable hotend and bed-heater safety timeouts capped at 60 minutes
    * PID settings screen for viewing, editing, autotuning, and resetting hotend/heatbed PID values
    * Optional BBF firmware signing for release builds with a local default signing key
    * Clarified stock Prusa bootloader limitations for custom firmware signing and release builds
    * Size-optimized release LTO linking to reduce flash usage, especially on XL
    * Additional machine-specific compile-time pruning for unused LED/status-light UI and driver paths
    * UI theme updates and theme import support
  * Fixes
    * Fixed serial print starts being missed while the printer is blocked by heater waits
    * Stopped homing and mesh-leveling commands from falsely entering serial-print mode; automatic fallback start detection now uses blocking heater waits
    * Fixed second serial prints started immediately after a finished print missing the serial print screen during startup heating
    * Fixed `M601` from serial hosts pausing motion without reliably parking the print head
    * Fixed serial-host `M601`/`M602` action handling so OctoPrint receives final `paused`/`resumed` confirmations without recursively triggering host pause/resume scripts
    * Fixed paused serial prints dropping out of the print screen
    * Fixed MMU filament runout recovery on serial prints leaving OctoPrint paused after the printer resumes
    * Fixed firmware/MMU/manual-intervention pauses during serial prints so bed heat remains protected, nozzle recovery can reheat before resume, and the host receives matching pause/resume actions
    * Fixed MMU error handling crash by deferring serial host pause/resume actions to the Marlin server loop instead of running them directly from the MMU reporting callback
    * Fixed serial MMU print completion leaving filament in the extruder by running the firmware final unload when streamed jobs lack reliable file metadata and by no longer treating progress-only `M73 P100 R0` reports as a hard end-of-print marker
    * Added host unit-test coverage for deferred MMU serial host pause/resume events
    * Fixed OctoPrint SD/USB uploads completing without data being written by entering upload mode immediately when serial `M28` is received
    * Fixed the serial print screen showing `Continue` instead of `Stop` during an active print
    * Fixed cancel confirmation wording on print screens
    * Fixed bed heating being disabled by the safety timer while a print is paused
    * Fixed serial progress jumping backwards when host and G-code progress disagreed
    * Fixed decimal `M117` progress parsing, for example `4.89% complete` now reports `5%`
    * Fixed host-reported serial print ETA snapshots staying frozen instead of counting down between updates
    * Added conservative host ETL parsing with streamed `M73 P/R` fallback when useful host percentage or ETA data is unavailable
    * Fixed time-only host messages such as `1:04 am` appearing on message screens
    * Fixed time-only host messages followed by `today` or `tomorrow` appearing on serial message screens
    * Fixed serial message pages hiding the numeric print percentage and retaining too few useful host messages
    * Fixed MMU load/unload and purge progress being hidden by the serial print screen during serial prints
    * Bridged additional print-adjacent FSM progress to the serial print screen, including safety-timer resume, nozzle-cleaning recovery, and cold-pull temperature progress
    * Added an upstream release playbook for quickly rebasing and rebuilding the RME firmware on new upstream Buddy releases
    * Added a configurable solid-green status LED hold after post-print filtering completes
    * Fixed post-print door acknowledgement when a Core One print finishes while the door is already open
    * Fixed chamber LEDs staying on through filtering when they should be allowed to time out
    * Fixed disabling chamber lights also disabling the status LEDs/status bar on Core One
    * Fixed canceled/aborted prints showing the finished green blink instead of the abort indication
    * Added Core One door-cycle acknowledgement to hide the remaining filtering status blink and finished hold
    * Added a Core One door-open prompt to end post-print filtration early; it closes automatically if filtering finishes while the prompt is open
    * Fixed closing the Core One door while the early-filtration prompt is open from also dismissing the underlying persistent print-finished screen
    * Added a finished-screen `Stop Filter` action that ends post-print filtration early without dismissing the finished screen
    * Added swipeable page-dot navigation for serial print-finished duration, completion-time, and remaining-filtration summaries
    * Fixed canceled prints losing their elapsed print duration during abort cleanup
    * Fixed active-print startup markers such as `M75` arming a second serial print start after `M77`, which reset completed serial-print duration to zero
    * Fixed serial `M77` completion returning to the home screen or leaving later serial commands ignored; completion now finalizes before queueing, returns `ok`, and clears the serial command pause gate
    * Fixed short ignored serial macros leaving the persistent finished screen or status LED in the wrong post-print state
    * Fixed the serial-print header flickering because its unchanged caption was redrawn on every GUI loop
    * Reduced serial message-page switching redraws by invalidating only widgets whose visibility changes instead of repainting the entire screen
    * Fixed canceling a detected serial print before its initial home from issuing an unnecessary unhomed Z clearance move
    * Fixed status LEDs remaining off after the finished hold instead of following normal active, idle, and deep-idle brightness after screen or door activity
    * Fixed the persistent print-finished screen keeping chamber lighting and the LCD awake throughout post-print filtration
    * Fixed external GPIO light bar active-sink output handling and per-state sequence behavior
    * Fixed external chamber light flicker caused by transient off/on commands during print start and finish transitions
    * Fixed active-state screen brightness being configurable too low to safely read the display
    * Fixed dim idle/deep-idle screens accepting accidental first input before the printer has visibly woken
    * Changed `Off` screen brightness to disable the LCD controller backlight path instead of only writing minimum brightness
    * Kept OOB/setup, calibration, self-test, MMU, and other guided FSM flows in the active lighting/screen state
    * Fixed all-build progress display leaving a completed job shown as running at 100%
    * Fixed XL release boot overflow while keeping the same RME feature set enabled as the other supported printers
    * Added `M154` RME fleet-configuration G-code and Settings export to `/usb/rme_settings.gcode`
    * Fixed PID autotune save/load behavior so `M303 ... U1` followed by `M500` persists tuned PID values for future boots
    * Increased chamber vent open/close lever travel slightly to help overcome slide resistance [C1, C1L]
    * Suppressed manual open-vent prompts during serial prints and on Core One Plus
    * Made Core One Plus automatic vent handling defer to explicit `M870` commands and made serial prints rely on streamed vent commands instead of inferred filament state
    * Fixed Core One Plus selection reporting as Core One L; Plus now reports `COREONE+`, while Core One L builds no longer expose the Core One / Plus selector
    * Fixed cold-boot manual Z jogging crashing when PID persistence skipped the upstream Marlin motion and stepper initialization path
    * Fixed the top-level release wrapper so managed `.venv` tools, including Nunavut `nnvg`, are automatically visible to CMake child builds
    * Protected GPIO pins reserved for the external light bar from generic GPIO reconfiguration commands
    * Fixed Prusa Connect feature gating caused by the custom `-RME` firmware suffix; Connect registration, telemetry/events, and websocket requests now report the upstream-compatible firmware version

This is a custom firmware release based on upstream Prusa Firmware Buddy 6.5.7. It focuses on serial printing, OctoPrint usability, print-finished handling, LED behavior, chamber filtration, and external light bar control.

The release is based on upstream tag `v6.5.7` (`7119a302d6`) with the RME changes replayed on branch `rme-v6.5.7`.

The running firmware UI uses the short `6.5.7-RME` version. Prusa Connect intentionally receives the stock upstream `6.5.7` version string for compatibility and feature gating, while local UI/API surfaces and release artifacts keep the RME identity. The stock Prusa bootloader firmware-selection menu still appends the BBF header build number; removing that numeric suffix requires a bootloader change because the build number is mandatory update-order metadata.

## Upstream 6.5.7 base

This RME release includes the upstream Buddy changes from `v6.5.3` through `v6.5.7`, including the xBuddy early-BOM boot-crash fix, logging deadlock prevention, Connect certificate handling, status LED finishing/aborting state updates, nozzle-cleaner and parking changes, input-shaper calibration fixes for CoreXY printers, modular-bed preheat fixes, and heater-timeout dialog guards for builds without human interaction support.

## Serial printing screen

A dedicated serial printing screen has been added for OctoPrint and other serial hosts. The screen shows print progress, printer status, filtered host messages, and a print-finished state with a `Continue` acknowledgement button.

Serial print start detection handles `M75`, eligible `M73 P0/Q0`, OctoPrint-style startup/status messages, and blocking heater waits such as `M109`, `M190`, and `M191`. Homing, mesh leveling, and ordinary toolhead movement no longer enter serial-print mode by themselves.

Starting a second serial print immediately after a previous print finishes now clears the previous finished/aborted state before entering the new serial print state.

Serial print completion is detected from explicit `M77`. Serial `M77` is handled before it enters the normal G-code queue, returns `ok` to the host, keeps the persistent finished screen active, and clears the serial command pause gate so later host commands and the next `M75` are accepted. Progress-only `M73 P100/Q100 R0/S0` reports are not treated as a hard end marker because they can arrive before streamed end G-code, including MMU unload commands, has finished.

Serial pause handling has been improved. `M601` from OctoPrint or another serial host now stops accepting additional streamed commands while the existing queue drains, then runs the normal print-head parking sequence. Serial commands are accepted again after the printer reaches the paused state so `M602` or host resume can be received.

For serial-host initiated `M601` and `M602`, the firmware no longer echoes new `//action:pause` or `//action:resume` requests back to the host that sent the command. It still reports `//action:paused firmware_pause` after the head is actually parked and `//action:resumed` after reheating/unparking has returned the printer to the printing state. This keeps OctoPrint state synchronized without recursively invoking OctoPrint pause/resume scripts or host-side heater-off actions.

Firmware pause states, MMU errors, and runout-style pauses keep the print treated as active for the print screen and chamber lighting. They also hold the bed-heater safety timer reset so the bed does not cool during a recoverable paused print. The printer reports pause/resume style host actions back to serial hosts where applicable.

MMU filament runout and other manual-intervention recovery on serial prints can restore the nozzle target before resume when needed and now send the serial-host resume action after the printer resumes, so hosts such as OctoPrint do not remain paused after the printer has recovered.

Print-time runout now also watches the toolhead filament sensor when an MMU, side, or external filament sensor is the primary runout source. This lets filament breaks or jams between the upstream sensor and the extruder trigger the normal M600 pause instead of relying only on the upstream sensor.

Loadcell stuck-filament detection remains configurable from `Stuck Filament Detection` and `M591 S`. A separate `Filament Movement Detection` setting and `M591 U` control the new movement/tangle protection, while both settings share the same loadcell E-stall detector and M1601 pause path.

At serial print finish, MMU-enabled printers run the firmware final unload when the MMU still reports filament present even if the streamed job did not provide scanned file metadata. This prevents OctoPrint-style serial jobs from leaving filament in the extruder when slicer metadata is unavailable or host progress reports reach 100% before the final end-gcode unload sequence is complete.

The serial print screen now has selectable UI modes:

  * `Legacy` shows the OctoPrint-style logo screen.
  * `Messages Only` shows the full-screen serial messages view.
  * `Progress` is the default new serial print UI with progress, status, and messages.

The UI mode is selected from a dropdown, not multiple independent toggles.

When a serial print finishes, the screen remains on `PRINT FINISHED` until the user acknowledges it or another print starts. The summary rotates through print duration, completion time, and remaining post-print filtration time when filtering is active. The page-dot indicator shows the available summaries and the user can swipe through them manually. Normal-print finished screens expose the same timing information.

Elapsed print duration is preserved continuously while a print is active. Serial-host end commands and cleanup G-code cannot reset the displayed duration to zero before either serial or normal print-finished summaries are rendered.

Startup markers such as `M75`, startup `M73`, and OctoPrint startup text received while a serial print is already active no longer arm a pending second serial print start. This prevents trailing end G-code after `M77` from restarting serial-print state and clearing the completed duration before the persistent finished screen renders it.

Very short serial macro prints, such as a quick `M75`/`M77` pair that never produced a useful printed-Z height, are ignored as non-print activity and restore the printer's previous print screen and status LED state. If the previous state was an unacknowledged finished print with a visible print screen, the finished screen and indication remain; if no print screen was visible, the printer returns to the home/idle state.

Elapsed duration includes startup heating time. File and serial print timers start before startup G-code continues through blocking hotend, bed, or chamber heating waits.

Canceled prints preserve the elapsed print duration captured before abort cleanup begins. Serial-print header captions are now updated only when the print state changes, preventing redraw flicker during normal GUI loops.

## OctoPrint SD/USB storage support

OctoPrint can now use the printer storage path for print-from-SD style workflows against the printer's USB media.

The firmware now advertises SD-card support through `M115` capabilities and implements the host-facing SD command set needed by OctoPrint and similar serial hosts:

  * `M20` lists printable files recursively from `/usb/`, including file sizes and long-name output with `M20 L`.
  * `M21` reports media availability.
  * `M23` selects a file from USB media.
  * `M24` starts the selected file or resumes a paused print.
  * `M25`, `M26`, and `M27` pause, set byte position, and report byte-position progress.
  * `M28`/`M29` write uploaded G-code lines to USB media without feeding those lines into the serial-print detector.
  * `M30` deletes files from USB media.
  * `M32` selects and starts a file in one command.

The command handlers normalize host paths onto `/usb/`, reject parent-directory traversal, create upload directories as needed, and keep uploaded file contents out of the live serial print stream until the host selects and starts the uploaded file. Serial `M28` starts upload mode immediately as it is received, so fast OctoPrint streams cannot outrun the queued command handler and have their file lines acknowledged without being written.

## Prusa Connect compatibility

Prusa Connect registration, telemetry/events, and websocket upgrade requests now report the base upstream firmware version instead of the custom `-RME` suffix. This keeps Connect's cloud-side compatibility checks and feature gates aligned with the upstream firmware base while preserving RME branding on the printer UI, local PrusaLink version endpoint, M115 output, crash dumps, BBF metadata, and release artifacts.

Connect file uploads still depend on writable USB media. The printer advertises `/usb` storage to Connect only when media is inserted and mounted, and Connect downloads remain constrained to `/usb` paths and transferable file types.

Serial-printing UI screens no longer mask the underlying print lifecycle from Prusa Connect. While a serial print is active, Connect telemetry and state events report the same `PRINTING`, `PAUSED`, `ATTENTION`, `FINISHED`, or `STOPPED` stages that normal file prints use instead of collapsing the printer to generic `BUSY`.

## Host progress reporting

Serial print progress now prefers progress reported by the serial host over firmware fallback progress. This avoids the display jumping backwards when the host and the G-code stream disagree. Trusted host progress comes from host status messages such as OctoPrint `M117` status text; streamed `M73` remains available for print start/end detection and fallback progress, but no longer overrides fresh host-reported percent or ETA.

Decimal host progress messages are parsed correctly. For example, `M117 4.89% complete` now reports `5%` instead of being interpreted as `89%`.

## Serial host message filtering

Progress, ETA, and time-only messages are filtered from the message screen so useful host messages remain visible. This includes bare clock messages such as `1:04 am` and host clock messages followed by `today` or `tomorrow`.

The serial message page keeps the progress bar visible, shows the current print percentage in the message header, uses a denser message font, and retains more useful host messages instead of dropping back to only the newest message when the buffer fills.

During MMU load/unload and purge steps, including extra purge during serial prints, the serial status page now shows the firmware FSM progress percentage so the on-printer progress bar is not left at `0%` while the filament action is running.

The serial status page also bridges other print-adjacent FSM progress sources where they are active while the print screen is visible: safety-timer temperature resume, nozzle-cleaning recovery progress, and cold-pull heating/cooling progress.

## Post-print LED indication [C1, XL, MK]

Status LEDs smoothly pulse from the configured warning color toward RGB white as the post-print filtration timer expires unless the completed print is acknowledged by a Core One chamber-door open/close cycle. With the default palette, the pulse starts yellow and becomes progressively whiter as an approximate at-a-glance indication of chamber air cleanup progress. The pulse reverses immediately at its minimum rather than dwelling fully off. When filtration ends, or immediately after a print that does not need filtration, unacknowledged status LEDs hold solid green for the configurable `Status Finish Hold` duration. The default is five minutes. The distinct colors make active filtering and completed filtering recognizable without relying on the animation alone. After the hold expires, status LEDs return to their normal idle sequence.

Mid-print filtration fan operation does not activate the green filtering indication. The status LEDs remain in their normal blue printing state until the print has finished, and chamber-door acknowledgement is ignored until then.

Chamber/side lights remain independent from that status indication. On Core One and Core One L, the chamber door can still wake chamber lighting and acknowledge its held chamber-light state. A door open/close cycle during filtration also acknowledges the status indication. Chamber/side lighting and the LCD resume normal idle timing while post-print filtration continues.

The persistent print-finished screen exposes a `Stop Filter` action while post-print filtration remains active. It ends filtration without dismissing the completed-print summary; `Continue` or `Home` remains a separate acknowledgement action and resets the status LED finished/filter indication centrally. Opening the Core One or Core One L door during filtration prompts whether to end the sequence early. If filtering naturally completes while that prompt is open, the prompt closes automatically and returns to the persistent print-finished screen. Closing the door while the prompt is open dismisses only that prompt and does not close the underlying print-finished screen.

`Status Finish Hold` is available in Lights Settings. Fleet configuration can set the same value with `M154.7 H<seconds>`, where `H0` disables the solid-green hold.

After that hold, the finished animation remains selected by the print state while its brightness follows the normal active, idle, and deep-idle sequence. Screen interaction and Core One door activity wake the strip again. An open Core One door holds active brightness until the door closes, then the normal timeout sequence resumes.

## Chamber and side LED timeouts [C1, XL]

The chamber/side LED timeout behavior has been simplified around two user-facing timings:

  * Time until dimmed
  * Time until off

The off time is measured from the last activity until the LEDs turn off. The dim time is measured from the last activity until the LEDs dim, capped by the off time.

Door-open activity on Core One keeps the active timer reset while the door is open. After the door closes, the active countdown begins again.

Serial movement or non-print serial activity counts like screen activity for the LED timers. `M153` can be sent by a serial host to mark incidental non-print work idle. It is ignored while a print is active or paused, so it cannot suppress chamber/side lighting during a print.

During an active print, the Tune menu includes a temporary percent control for screen brightness. Printers with chamber/side LEDs include a temporary percent control for those lights, and printers with a status strip include a temporary percent control for the status LEDs. `0%`/`Off` disables the corresponding lighting for the current print, while non-zero values dim it independently. Chamber/side LED overrides do not change the LCD state or screen brightness. These per-print overrides reset after the print and do not overwrite the persistent lighting defaults.

The chamber/side lighting control is available on printers with `HAS_SIDE_LEDS`, and the status LED brightness control is available on printers with `HAS_LEDS`.

LED brightness and enable values are independent between `Deep Idle`, `Idle`, `Active`, and `Printing`. There are no cross-state ordering limits: for example, Printing exterior lights may be dimmer than Idle lights or fully disabled while Idle lights remain enabled. The only state brightness minimum is the `15%` LCD safety floor for Active and Printing screen brightness. `M153` is limited to non-print host activity.

Screen brightness is also configurable per state on printers with supported display backlight control. Active and Printing screen brightness settings are clamped to at least `15%` so the printer cannot become effectively unreadable while active or printing. Idle and Deep Idle can be set to `Off`. If the idle or deep-idle screen brightness is below `15%`, the first touch or encoder input only wakes and brightens the screen; that input is consumed and does not activate the focused UI action. If a temporary print screen override is below `15%`, the first interaction raises the screen to the readable `15%` print wake floor and subsequent interactions remain responsive until the print ends or the override changes.

When screen brightness is `Off`, supported display paths write a full black frame to the panel before disabling brightness/backlight control and display output. ST7789/MINI also enters sleep-in. ILI9488/XLCD also forces the front/status LED strip dark while the screen is off so XLCD revisions with a different backlight channel cannot remain lit. While the screen is intentionally off, display-driver pixel writes are suppressed so delayed UI draws cannot repaint the old UI over the black frame. Displays re-enable write access and brightness control before setting a non-zero brightness, and the first wake from `Off` invalidates the full current screen so MINI/Core One/XL redraw immediately instead of remaining black until the next UI event.

During first boot after flashing, bootstrap/install screens hold screen brightness at 100% until resource, bootloader, ESP, and puppy setup is complete. The firmware now paints the splash screen before starting those install steps, and the post-boot idle timer is seeded only after the first home-screen paint so the printer cannot dim immediately based on pre-display boot time.

OOB setup, calibration, self-tests, MMU tests/actions, and other guided FSM screens are treated as active use. Those flows hold the active lighting and screen brightness state so the printer does not dim or turn the screen off during setup or maintenance workflows.

The Lights Settings menu now exposes state-specific LED pages:

  * `Deep Idle`
  * `Idle`
  * `Active`
  * `Printing`

Each state page can enable or disable the chamber/side LEDs for that state and set the chamber/side LED brightness used by that state. On supported xBuddy GPIO breakout hardware, each state page can also enable or disable the external light bar independently from the chamber/side LEDs.

This makes sequences such as chamber/side LEDs on for idle, active, and printing while the external light bar is enabled only during printing configurable from the UI.

The main Lights Settings page controls the timeout sequence. `Active to Idle` controls how long the printer remains in the active light state after user or serial activity before entering the idle lighting state. `Idle to Deep Idle` controls how long the printer remains idle before entering deep idle/off. The active page also includes a `Door Holds Active` setting so Core One users can decide whether an open chamber door keeps the printer in the active lighting state.

The main Lights Settings page includes `Door Finish Ack` on Core One and Core One L. This controls whether the chamber door is used to acknowledge the post-print finished-light hold.

## Build size and flash headroom

Release builds now use `-Oz` for release compile and final LTO link optimization, plus `-fmerge-all-constants` to merge duplicate constants and strings. This reduces final firmware flash usage across platforms and gives XL enough room to keep the same RME features enabled as the other supported printers.

With XL per-print side-strip percent brightness, per-print screen brightness, per-state screen/status brightness, and OctoPrint SD/USB storage support enabled, the local XL release boot image now builds successfully with substantial boot flash headroom by moving XL translations to the resource image.

Additional machine-specific compile-time pruning keeps unused code out of targets that cannot use it. MINI no longer instantiates the status LED color settings screen when `HAS_LEDS` is disabled. Core One/Core One Plus compile the status-bar-off LED path directly instead of carrying a runtime branch, and XL compiles the side-strip/enclosure second-driver path only for XL.

Built-in UI themes now apply their status LED palette immediately. Indigo uses the calibrated `#1A0040` deep-indigo printing status bar while the other built-in profiles explicitly retain the standard cyan status bar. Imported JSON themes can set the in-progress status-strip / light-bar color through `led.printing`.
AC-controller progress animations now use the configured printing status LED color instead of overriding it with a hardcoded cyan value.
Core One xBuddy-extension RGBW PWM preserves raw `0-255` duty values on its dedicated timer. Custom `M150` status-bar animations remain active until the printer state changes, matching upstream behavior. Solid `M150 A0` calibration commands apply immediately without passing through the normal animation cross-fade. Solid-frame animation percentages are normalized before RGBW fading so intermediate channel values no longer wrap modulo 256; the analog light bar can now use the colors supported by the hardware instead of collapsing intermediate values into unrelated colors.

The per-state LED settings pages now share one runtime menu container instead of four separately instantiated template menus. This preserves Deep Idle, Idle, Active, and Printing controls while reducing generated GUI code enough for XL to keep the full feature set.

Focused release boot builds after the latest pruning:

  * XL: `1291244 B / 1919 KB` flash, `65.71%`
  * MINI: `893296 B / 895 KB` flash, `97.47%`
  * Core One: `1290800 B / 1919 KB` flash, `65.69%`
  * Core One L: `1290944 B / 1919 KB` flash, `65.69%`
  * MK3.5: `1850136 B / 1919 KB` flash, `94.15%`
  * MK4: `1919312 B / 1919 KB` flash, `97.67%`

`--final` builds retain the `-RME` full and short version suffixes while removing numeric development suffixes from the running application firmware version. The installed firmware UI identifies the custom firmware as `6.5.7-RME`. The stock Prusa bootloader firmware-selection screen still appends the mandatory BBF header build number after the `RME` tag; removing that bootloader-rendered number requires a bootloader change. Final builds disable `DEVELOPMENT_ITEMS` by default unless explicitly overridden. Final/non-development builds keep the full `M503` report command enabled, including human-readable headings/comments and TMC settings. Replayable persistent settings remain reportable, including PID, motion, probe, and other saved configuration values. PID editing, autotune, save, and load commands remain available through `M301`, `M303`, `M500`, and `M501`. Core One, Core One L, XL, and MINI store translations in the external resource image. The Core One/Core One L/XL resource image size is large enough to fit the web assets, puppy firmware, QOI assets, MMU firmware where applicable, and translation files.

An upstream release playbook has been added at `doc/rme_upstream_release_playbook.md`. It documents the RME feature groups, conflict hotspots, build commands, smoke-test matrix, flash budget checks, release-note requirements, and BBF publishing checklist for quickly rebuilding the RME firmware on top of future upstream Buddy releases.

## Safety timer and paused prints

Hotend and bed-heater safety timeouts are now grouped under `Settings -> Heater Safety`. The hotend timeout is persistent and configurable from the UI or with `M86 S<seconds>`. The bed timeout remains independently configurable from the UI or with `M86 B<seconds>`.

Both settings are capped at 3600 seconds / 60 minutes. Hotend timeout values are clamped to a non-zero safety value. `M86 B0` still disables the bed-heater safety timeout; non-zero bed values are clamped to the same 60-minute maximum and a minimum of 3 seconds.

While a print is active, paused, pausing, or otherwise still in a print state, the bed-heater safety timer is held reset. This prevents the bed from being turned off during a long paused print. The nozzle safety timeout behavior remains separate, and serial pause recovery can restore the nozzle target before sending host resume/resumed actions.

## PID settings and autotune persistence

`Settings -> PID Settings` now sits next to Input Shaper and Phase Stepping. PID Settings opens separate Hotend and Heatbed submenus. Each submenu shows that heater's PID values, where supported, and each value can be edited directly from the printer UI. The hotend and heatbed can each be reset independently to their firmware defaults.

PID values changed from the UI are applied immediately and stored persistently.

Supported heaters can also be autotuned directly from the PID Settings screen. The autotune screen shows heating/cooling state, temperature, cycle progress, and the resulting P/I/D values. When autotune completes, the user is prompted to save or discard the new values; saving applies them and persists them, while discarding leaves the existing PID settings unchanged. Failed autotunes report failure instead of offering to save incomplete values.

PID autotune remains available through Marlin G-code. Use `M303 ... U1` to apply the autotuned values to the live heater PID settings, then `M500` to save them to persistent storage. Saved values are loaded again on future boots through the normal settings-load path, and `M501` can reload them manually.

## Filtering LED indication [C1, XL]

When chamber filtration is active, the status LEDs use a slow warning-color brightness pulse. This is intentionally distinct from the solid green finished hold and is shared by printers with `HAS_CHAMBER_FILTRATION_API`, including Core One and XL.

The chamber/side LEDs are allowed to dim or turn off during the filtering step. Status LEDs remain in the filtering indication unless a Core One chamber-door open/close cycle acknowledges the completed print. Deep idle does not suppress an unacknowledged filtering status blink.

The filtering indication is strictly post-print. Running filtration fans during an active print does not replace the normal blue printing animation, and door acknowledgement cannot dismiss any finished or filtering indication before the printer reaches `Finished`.

Opening and closing the Core One chamber door during filtration acknowledges the completed print. It forces the status LED output fully off for the rest of filtration, even if idle status LEDs are configured with a non-zero brightness or color, and suppresses the later solid-green status hold until the next print starts. The chamber-light hold is acknowledged at the same time.

## External GPIO light bar control [C1, MK]

External GPIO light bar support has been added for supported xBuddy boards with the GPIO breakout / IO expander, including Core One / Core One Plus and MK-series xBuddy printers.

The light bar can be configured from the UI, including selectable GPIO pins and per-pin output mode. Pin capabilities are respected so only valid output modes are selectable for each pin.

GPIO pins assigned to the external light bar are protected from generic GPIO configuration commands.

External light bar pins intentionally ignore PWM brightness. When enabled for the current state, configured external light pins are fully on; when disabled for the current state, they are off.

The external light bar is configured per lighting state from the Lights Settings state pages. This allows the chamber/side LEDs and external GPIO light bar to be used together or independently. For example, chamber/side LEDs can be on during idle, active, and printing, while the external light bar is only enabled during printing.

During an active print, the temporary chamber/side light override is the only control that intentionally pairs chamber/side LEDs with the external GPIO light bar: `0%` turns both off for that print, and any non-zero override allows the external light bar to follow its print-state enable setting. Persistent per-state chamber/side LED settings and external GPIO enable settings remain independent.

The external light output is latched and off-debounced so short firmware state gaps during print start or print finish do not command the external chamber lighting off and immediately back on. Sustained off states still turn the external light off normally.

The external light bar is forced off during manual belt tuning because GPIO external lighting does not support PWM and can interfere with the belt-tuning light behavior. The force-off is not latched; when belt tuning exits or is aborted, the external light bar is recomputed on the next LED update and returns to the state configured for the printer's current lighting mode.

Active-sink pins are driven low when the external light is on. Pins that only support pull-down/floating output cannot be configured as active-high outputs. IO expander outputs are returned to their factory-safe state during shutdown/reset handling.

## LED G-codes

The chamber/side LED G-code behavior has been updated:

  * `M151 A<0-255>` sets active/user/door side LED brightness.
  * `M151 P<0-255>` sets print/minimum side LED brightness.
  * `M151 L<0-255>` sets dimmed/idle side LED brightness.
  * `M151 I<seconds>` sets time until dimmed.
  * `M151 O<seconds>` sets time until off.
  * `M153` marks incidental non-print host activity idle. It is ignored while a print is active or paused.
  * `M152 P<pin> S<0|1> A<0|1>` configures external GPIO light bar pins on supported xBuddy boards.
  * `M152 P<pin> F<0|1|2>` applies an external-light diagnostic override: `F1` forces on, `F0` forces off, and `F2` clears the override.
  * `M86 S<seconds>` sets the hotend safety timeout.
  * `M86 B<seconds>` sets the bed-heater safety timeout. `B0` disables the bed-heater safety timeout.

## RME fleet configuration G-code

RME settings can be applied from G-code so a shared settings file can configure a fleet of printers:

  * `M86 S<seconds> B<seconds>` sets exported hotend and bed-heater safety timeouts.
  * `M154.0 D<0-100> I<0-100> A<15-100> P<15-100>` sets screen brightness for Deep Idle, Idle, Active, and Printing.
  * `M154.1 D<0-100> I<0-100> A<0-100> P<0-100>` sets chamber/side brightness where `HAS_SIDE_LEDS` is available.
  * `M154.2 D<0-100> I<0-100> A<0-100> P<0-100>` sets status LED brightness where `HAS_LEDS` is available.
  * `M154.3 A<seconds> E<seconds> O<seconds> H<0|1> F<0|1>` sets lighting timeouts and door/post-print behavior where side LEDs are available.
  * `M154.7 H<seconds>` sets the status LED solid-green finished-hold duration where status LEDs are available.
  * `M154.4 U<0|1|2> T<seconds> A<0|1> S<0|1>` sets serial printing UI mode, timeout, auto-start, and screen enable.
  * `M154.5 D<0|1> I<0|1> A<0|1> P<0|1>` sets external GPIO light-bar state enables where supported.
  * `M154.6 E<index>` sets Core One / Core One Plus extended printer type where supported.
  * `M154.8` starts a configured post-print filtration cycle where chamber filtration is supported; `M154.8 S0` stops the active cycle.

The Settings menu includes `Export RME Settings`, which writes the current persistent RME settings to `/usb/rme_settings.gcode` for easy cloning to another printer.

Compatibility note: `M151 W...` is no longer used as the chamber/strip brightness setter. Use `A`, `P`, or `L` depending on the intended brightness state.

## UI and themes

This release includes a broader UI theme refresh, updated icons and image assets, theme JSON support, and theme import/validation support.

It also adds status LED color settings, serial printing settings, external light bar settings, lock settings, grouped heater safety timeout settings, and improved PIN/text input behavior.

Waiting and heating status text on print screens has been made more visible.

## Chamber fan and filtration controls [C1, XL]

Chamber fan and filtration controls have been expanded with additional menu integration and clearer state handling.

Filtering-specific LED behavior has been added so filtration can continue after a print while the chamber/side LEDs follow their normal timeout behavior.

Filter lifetime usage is committed when filtration stops or is disabled, including manual early-stop actions, so the final partial accounting interval is not discarded.

`M154.8` starts the configured post-print filtration cycle from G-code, using the same duration and power settings as the normal post-print path. `M154.8 S0` stops the active cycle.

## Automatic chamber vents [C1, C1L]

The automatic chamber vent open and close moves now travel 1.5 mm past the previous lever endpoint before releasing tension. This small overtravel is intended to help overcome resistance in the vent slide mechanism without making the move aggressive.

Regular Core One file prints keep the manual open/close vent prompts. Serial prints assume the vent is already positioned correctly and do not derive a vent action from the stored filament selection.

Core One firmware now exposes a printer type setting for `CORE One` and `CORE One Plus`. On Core One Plus file prints, automatic vent movement is used unless the scanned start G-code contains an explicit `M870` vent command. For serial Core One Plus prints, streamed `M870` commands are authoritative because the printer cannot reliably infer filament state from the host stream.

## Manual motion stability

Persistent PID loading now extends the upstream Marlin settings initialization path instead of replacing it. Boot and `M501` first restore the normal motion defaults, planner positioning state, global endstop defaults, and stepper-driver configuration, then overlay the persisted hotend and heatbed PID values. This fixes the cold-boot failure where moving Z before any other axis could drive the motors incorrectly and reboot the printer.

Periodic odometer writes were also removed from the nested server `cycle()` path. Print finalization remains the explicit persistence point, keeping EEPROM journal writes out of blocking manual-movement loops such as `G123`.


## Firmware signing

Release builds can optionally sign generated `.bbf` files by setting `FIRMWARE_SIGNING_KEY` when running the top-level release wrapper:

```sh
FIRMWARE_SIGNING_KEY=/path/to/private.key ./build.py
```

If `FIRMWARE_SIGNING_KEY` is not set, `./build.py` uses the machine-local default key at `.local/firmware-signing-key.pem` when it exists.

The underlying build wrapper also supports `--signing-key /path/to/private.key`.

The top-level `./build.py` wrapper defaults to at most four concurrent printer builds to avoid overwhelming the build machine. Use `--jobs N` to override the cap when appropriate. Interrupted wrapper builds terminate active child processes instead of leaving orphaned Ninja/LTO jobs running. Completed builds report each machine's flash usage, aggregate RAM usage, individual memory-region usage, total elapsed wall-clock time, and absolute staged BBF path. The wrapper also prepends `.venv/bin` to child build `PATH` and passes `Python3_ROOT_DIR` by default so managed virtualenv tools such as Nunavut `nnvg` are available during CMake configuration.

The wrapper can also build multiple maintained RME release branches in one command:

```sh
./build.py --final --jobs 14 --versions 6.5.7 6.6.1
```

This checks out each requested `rme-vX.Y.Z` branch through a cached Git worktree under `.rme-version-builds/`, runs that version's normal release wrapper, and stages artifacts under `bbf/X.Y.Z/`. The cached worktrees retain their `build/`, `.venv/`, and `.dependencies/` directories so future rebuilds of the same versions can reuse CMake, compiler, Python, and downloaded dependency state instead of starting cold.

Signing with a custom key does not bypass the official Prusa bootloader non-genuine firmware warning. The stock bootloader only trusts its built-in public key; a custom signature is useful for future private trust chains or custom bootloaders, but not for making a custom build appear genuine to an unchanged official bootloader.

The RME firmware builds target the stock Prusa bootloader, but the bootloader itself is not built or modified as part of this open firmware tree. Prusa publishes the Buddy firmware source, while the stock bootloader is distributed as a closed binary and the trusted private key is not available.

Because of that, these RME builds cannot be signed in a way that passes the official bootloader genuine-firmware check. The local signing key only produces internally consistent custom signatures.

## Supported hardware notes

  * Serial printing improvements are available where `HAS_SERIAL_PRINT` is enabled.
  * Side/chamber LED behavior is available where `HAS_SIDE_LEDS` is enabled.
  * Per-print chamber/side light disabling is available where `HAS_SIDE_LEDS` is enabled.
  * Per-print chamber/side light brightness overrides are available where `HAS_SIDE_LEDS` is enabled.
  * Per-print status LED brightness overrides are available where `HAS_LEDS` is enabled.
  * Per-print screen brightness overrides are available from Tune during active prints.
  * Per-state screen brightness settings are available where the display supports firmware backlight brightness control.
  * Active and Printing screen brightness settings are clamped to at least `15%`; Idle and Deep Idle can be set to `Off`.
  * `Off` screen brightness requests disable the display-controller backlight path where supported by the hardware.
  * Idle/deep-idle values below `15%` require one wake-only input before normal UI actions resume.
  * Filtering status LED behavior is available where `HAS_CHAMBER_FILTRATION_API` is enabled.
  * External light bar support and external sequence settings are intended for supported xBuddy boards with the GPIO breakout / IO expander.
  * Core One and Core One L use the chamber-door post-print acknowledgement flow for chamber lighting.
  * Status LEDs blink throughout filtering unless the completed print is acknowledged by a Core One chamber-door open/close cycle. Unacknowledged prints use the configurable timed solid-green finished hold before normal idle behavior resumes.

## Build validation

Recent local validation used the firmware build wrapper. The wrapper supplies the managed `.venv` path and `Python3_ROOT_DIR` automatically so nested CMake projects can find Nunavut `nnvg`.

```sh
python3 build.py --final --jobs 4 --no-store-output
```

The XL release path was revalidated after the nested puppy-firmware Python propagation fix and the expanded config-store visitor generation:

```sh
./build.py --preset xl --jobs 14 --final --store-output
```

Result:

```text
1 succeeded, 0 failed
xl_6.5.7-RME.bbf -> bbf/xl_6.5.7-RME.bbf
```

Final validation result:

```text
14 succeeded, 0 failed
```

Validated release target set and package sizes:

```text
coreone     1.24 MiB / 1.87 MiB flash (66.01%), 179.8 KiB / 269.1 KiB RAM (66.82%)
coreonel    1.24 MiB / 1.87 MiB flash (66.00%), 181.1 KiB / 269.1 KiB RAM (67.28%)
mini        876.7 KiB / 895.0 KiB flash (97.96%), 148.2 KiB / 192.0 KiB RAM (77.18%)
mini-en-cs  877.4 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-de  877.4 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-es  877.4 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-fr  877.4 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-it  877.4 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-pl  877.4 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-ja  885.0 KiB / 895.0 KiB flash (98.88%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mini-en-uk  877.5 KiB / 895.0 KiB flash (98.04%), 148.2 KiB / 192.0 KiB RAM (77.20%)
mk4         1.83 MiB / 1.87 MiB flash (97.89%), 172.3 KiB / 260.0 KiB RAM (66.29%)
mk3.5       1.77 MiB / 1.87 MiB flash (94.42%), 161.8 KiB / 260.0 KiB RAM (62.24%)
xl          1.24 MiB / 1.87 MiB flash (66.00%), 188.3 KiB / 268.1 KiB RAM (70.23%)
```

The final staged BBF set is:

```text
coreone_6.5.7-RME.bbf
coreonel_6.5.7-RME.bbf
mini_6.5.7-RME.bbf
mini-en-cs_6.5.7-RME.bbf
mini-en-de_6.5.7-RME.bbf
mini-en-es_6.5.7-RME.bbf
mini-en-fr_6.5.7-RME.bbf
mini-en-it_6.5.7-RME.bbf
mini-en-pl_6.5.7-RME.bbf
mini-en-ja_6.5.7-RME.bbf
mini-en-uk_6.5.7-RME.bbf
mk4_6.5.7-RME.bbf
mk3.5_6.5.7-RME.bbf
xl_6.5.7-RME.bbf
```

Earlier focused validation for this patch stack also covered the host `mmu_tests` unit-test target with `110041` assertions in `4` test cases. The signing path was validated with a temporary ECDSA key:

```sh
python3 utils/build.py --preset coreone --bootloader yes --skip-bootstrap --no-store-output --signing-key /path/to/private.key
```

That build produced a non-zero BBF signature.

## Changelog base

Comparison base: upstream `v6.5.7` (`7119a302d6`)

Current branch: `rme-v6.5.7`

Latest replayed RME commit: `c0f0a2ef`

Port-completion commits: `Finalize 6.5.7 RME release port`, `Fix Prusa Connect serial print state reporting`, `Fix serial MMU print completion unload`, `Fix serial M601 M602 host actions`, `Restore previous screen after ignored serial macro`, `Fix RME release build environment`, and `Keep toolhead runout active with upstream sensors`

The port-completion commits cover the upstream status LED state merge, the 256-field generated config-store visitor needed by XL, Prusa Connect serial-print state parity, serial MMU print-completion unload behavior, serial M601/M602 host-action synchronization, ignored short serial macro finished-screen restoration, release build environment fixes, and the secondary toolhead runout path for MMU/side/external filament sensor setups.

## Full changelog

Commits included on top of upstream `v6.5.7`:

```text
ac390cbcf  2026-06-24  new octoprint screen and better color scheme
ee9d8e698  2026-06-24  transfer features for previous patch set (not company specific)
3d77d5f28  2026-06-24  Ignore crckill build output
1fc4e1d60  2026-06-24  Fix terminal dirty flag buffer clearing
46f9f2b5e  2026-06-24  screen fixes
0db6f3bb3  2026-06-24  octoprint usability improvements
81ac96c3d  2026-06-24  add better led control to the printer
d72563729  2026-06-24  serial print overhaul
dc6168521  2026-06-24  fix possible bug
1d9295bf8  2026-06-24  better chamber fan controls.
1a2d3fe8b  2026-06-24  support other machines to
747385342  2026-06-24  fix print start and temp readings
88afee899  2026-06-24  version fixes
eb79f06fd  2026-06-24  massive ui overhaul
92fc56f72  2026-06-24  better detection
71a683db1  2026-06-24  better serial print screen
4a7de3554  2026-06-24  better reliability
7d8c3958d  2026-06-24  massive theme overhaul
747a0642e  2026-06-24  added theme validation for file import
a08f7dd22  2026-06-24  add build alias
47a62eb7c  2026-06-24  theme fixes
1f1b1fa7d  2026-06-24  fix some serial print issues
ad46fac18  2026-06-24  better build script
c653ac627  2026-06-24  better led control
985e322f5  2026-06-24  fix serial percent parsing
a566e60c1  2026-06-24  round up to the nearest percentage
08b425ebd  2026-06-24  more changes to les statuses and other features
17228c317  2026-06-24  led fixes for non core one printers
6a2bb8f09  2026-06-24  fix mini builds
1572dcbbc  2026-06-24  external led fixes for core one variants and for mk variants
8dd09b86c  2026-06-24  attempt to reduce first boot flicker
dbe2b711a  2026-06-24  some other misc fixes for led and print state
b5f8df5a2  2026-06-24  some other misc fixes
387d8a4b8  2026-06-24  even more misc fixes
071c55ac4  2026-06-24  even more misc fixes
35f6fea63  2026-06-24  led config overhaul
948fbff95  2026-06-24  code cleanup
d12ad9165  2026-06-24  code cleanup
96a0ce7d2  2026-06-24  some fixes
cbb810bbf  2026-06-24  more fixes
e3d31e7d1  2026-06-24  more fixes
32ba6f6ea  2026-06-24  more fixes
d0fea798d  2026-06-24  more fixes
a85100955  2026-06-24  more fixes
3c1cf0069  2026-06-24  better ui
f5626d2b9  2026-06-24  mini build fixes
4cea2e48b  2026-06-24  mini build fixes
41de8b123  2026-06-24  lots of bug fixes
421a03d7d  2026-06-24  more fixes and renaming
5a75b39b8  2026-06-24  xl fixes
0be2e8168  2026-06-24  size improvments and other fixes
dc2c0c839  2026-06-24  more fixes and added a playbook for future use
0a7df4aa5  2026-06-24  external lighting fixes
93d5414d0  2026-06-24  build fix for mk3.5
34109eac8  2026-06-24  playbook update
0d9248398  2026-06-24  lower flash usage
0a89d0d37  2026-06-24  update playbook
45a443abf  2026-06-24  updates for better leds and screen brightness control
866a551f0  2026-06-24  update playbook
3ec9f352a  2026-06-24  better screen brightness controls
b16d726c0  2026-06-24  update the playbook
68f76d144  2026-06-24  massive screen fixes
89931d0cc  2026-06-24  more fixes
f9bdb39e5  2026-06-24  fxi print screen and added support for saving custom pid c=values / changing them in the ui and restoring the factory defaults.
f4a96d025  2026-06-24  better flash managments and build script fizes
9c8b4618d  2026-06-24  playbook update
452e03370  2026-06-24  fix layout stuff
821c7530e  2026-06-24  fix print progress display
afdbcaf7b  2026-06-24  add auto pid process
d5a4e7128  2026-06-24  add auto pid process
ed31d20cb  2026-06-24  fixed print screen issues
147b8112e  2026-06-24  updated readme
c88052edc  2026-06-24  updated a lot of stuff for better filter control and more
56d1211f2  2026-06-24  led fixes for all systems
a73587e94  2026-06-24  better m503 for release builds
f1d3ac07f  2026-06-24  build script overhaul
ccce00e8b  2026-06-24  build script overhaul
3c87d88fe  2026-06-24  fxi the rme suffix
02c628fd8  2026-06-24  fix issues and core one vent behaviour as well as fixed the z move crash
90b5ddf31  2026-06-24  fix issues and core one vent behaviour as well as fixed the z move crash
12cd460b2  2026-06-24  fixes for the printing screen
013a08232  2026-06-24  fix filter usage tracking
57d41e030  2026-06-24  Gate filtration lighting until print completion
1cc0dd9ef  2026-06-24  Preserve print duration and restore status brightness cycling
8f20fb13c  2026-06-24  Apply theme status colors and smooth filtration pulse
93a9f71bb  2026-06-24  Migrate Indigo status LEDs away from cyan
e193ea2f5  2026-06-24  Keep Indigo LED color profile driven
5207758fb  2026-06-24  Keep non-Indigo printing LEDs cyan
ef8b9bb66  2026-06-24  Use saturated Indigo for status LEDs
6d8702f1d  2026-06-24  Darken Indigo status LED profile
58b670a0a  2026-06-24  Restore stable Indigo LED calibration
0e64b2576  2026-06-24  Stabilize Core One RGBW LED PWM
7c4d61ce7  2026-06-24  Linearize Core One RGBW strip PWM
c74bfa909  2026-06-24  Make solid LED calibration overrides immediate
f45bd71e3  2026-06-24  Preserve M150 overrides until state changes
1e68b4f73  2026-06-24  Restore reliable cyan printing status bar
a04fc57c1  2026-06-24  Fix RGBW lightbar color scaling and restore Indigo profile
331e6619b  2026-06-24  Fix serial print completion and refine status lighting
5fc693207  2026-06-24  Document serial completion and filtration lighting updates
497a4edb2  2026-06-24  Add Connect compatibility reporting and heater safety menu
4c76d6ce1  2026-06-24  Document Connect and heater safety updates
da061e385  2026-06-24  Split PID settings by heater
157d6a2e6  2026-06-24  Show filtration time in print finished stats
1aa060a7e  2026-06-24  Refresh release playbook audit numbers
4cda3ddcf  2026-06-24  Fix print lighting brightness overrides
2eef7d353  2026-06-24  Fix print lighting overrides and completion state
ff4c3c7fe  2026-06-24  Document print lighting override behavior
828ff0baa  2026-06-24  Update release notes for lighting override fixes
6ae1931e6  2026-06-24  Fix serial print finish and intervention handling
b59ac396e  2026-06-24  Document serial finish and intervention fixes
10877bfde  2026-06-24  Defer MMU serial host actions to server loop
0a9973211  2026-06-24  Document MMU serial host crash fix
fa6d213f9  2026-06-24  Fix OctoPrint uploads and add filtration trigger G-code
92f097066  2026-06-24  Document filtration trigger and upload fix
4513f700b  2026-06-24  Add MMU serial host event unit coverage
f3c3ec13e  2026-06-24  Document MMU unit coverage validation
a27af6d95  2026-06-24  Finalize 6.5.7 RME release port
a55f1f9ae  2026-06-24  Fix Prusa Connect serial print state reporting
5b32c93cf  2026-06-25  Fix serial MMU print completion unload
8301c1936  2026-06-30  Fix serial M601 M602 host actions
9b5d1361e  2026-07-02  Restore previous screen after ignored serial macro
b4ffeb2e5  2026-07-03  Fix RME release build environment
c0f0a2ef4  2026-07-06  Keep toolhead runout active with upstream sensors

```

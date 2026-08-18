# 6.6.3-RME Firmware for Prusa CORE One, CORE One INDX, CORE One L, XL, MK4, MK3.9, MK3.5 and MINI

## Summary

6.6.3-RME rebases the complete 6.6.2-RME feature stack documented below onto
Prusa Firmware Buddy 6.6.3. No 6.6.2-RME feature was intentionally removed.

### Additions since 6.6.2-RME

  * Material telemetry now reports the base polymer family independently from
    the selected profile. Connect and Link expose `material=PLA` with a
    separate `profile=PLA-00D`, and loaded-filament serial reports use
    `S"PLA"` plus `P"PLA-00D"`; profile identifiers no longer masquerade as
    filament types. Material-family storage and M865 `J` handling are enabled
    for every supported printer, rather than only the INDX variants. This does
    not enable INDX-only filament UI or calibration features on ordinary
    printers. Non-INDX models use spare bits in the existing filament-profile
    record, preserving the deployed config-store schema and startup layout.
    The new `@RME FILAMENT ASSIGN` operation independently changes a loaded
    tool's `material` and `profile`; omitted fields are preserved, preventing
    profile aliases such as `PLA-00D` from overwriting the `S"PLA"` material.
    `FILAMENT SET` and `CREATE` also accept the explicit `material=` spelling.

  * Fixed M976 batch validation and default PA fallback selection for custom
    filament profiles: slicer material fields now match the configured base
    material family (such as `PETG`) instead of the custom preset identifier.
    Exact custom names remain accepted as compatibility aliases.

  * Made firmware-candidate status non-blocking. Verified uploads now persist
    a private size/SHA cache, while legacy candidates are hashed in bounded
    scheduler slices with `state=validating` progress, a 15-second storage-read
    deadline, and explicit read/hash/timeout errors. Serial G-code and RME
    traffic remain responsive throughout validation.

  * Added independent filament-manufacturer tracking with 50 common built-ins,
    eight persistent keyboard/RME-created names, case-insensitive host matching,
    and selection alongside material and color during load and preload.
  * Uses bounded streaming USB CDC FIFOs for fast file and firmware transfers
    and advertises 1,000,000 as the normal-mode rate, while preserving ordinary
    G-code, the older RME protocol, and slower host settings as fallbacks.
  * Added negotiated four-frame RME bulk upload windows with 384 decoded bytes
    per frame, cumulative acknowledgements, and unchanged atomic/SHA safeguards.
  * Added raw USB upload with eight in-flight CRC32 frames, eliminating
    Base64 and line parsing while retaining final SHA-256 and atomic rename.
  * Added a white header badge with an Indigo/theme-tinted R for negotiated RME
    hosts and integrated file/firmware activity with the transfer indicator.
  * Added a USB-confined `@RME FILE` service for directory listing, metadata,
    Base64 chunk downloads, SHA-256-verified atomic uploads, abort cleanup,
    directory creation, rename/delete, print queueing, and normal validated BBF
    flash handoff. Internal storage is not exposed.
  * Expanded RME filament synchronization to create complete persistent local
    profiles with upstream base material, temperatures, chamber bounds,
    filtration, abrasive, flexible, and visibility properties.
  * Made session keepalives and read-only polling passive: they maintain the
    communications lease without holding the printer in an active device state,
    and structured state events now report the real firmware device state.
  * Expanded structured workflows to cover native filament load/unload,
    chamber-vent actuation, filtration, and every stable MMU phase, with
    rate-limited progress updates and deduplicated not-responding errors.
  * Added read-only out-of-band `@RME STATS QUERY` records for lifetime XYZ
    travel, extrusion and print time, current-job time, jobs started, MMU
    changes, tool picks, filtration use, supported waste-bin counts, crashes,
    power panics, and separate MMU failure counters.
  * Failed release presets now retain complete compiler output under
    `.rme-build-errors/<version>/<preset>.log` for multi-version builds.

  * Added an out-of-band `@RME` serial protocol that is handled before the
    motion G-code FIFO and therefore remains responsive during blocking heater
    waits, probing, MMU work, calibration, and queued serial printing.
  * Added volatile remote UI navigation, lock-aware control, theme and lighting
    configuration, persistent filament-preset synchronization, machine topology,
    build-envelope, hotend-count, and live motion-limit discovery.
  * Added generic dialog discovery and named recovery responses for MMU errors,
    tool-change failures, filament runout, stuck filament, PA calibration,
    probing, heating, firmware update, and purge-bin workflows.
  * Added negotiated event subscriptions with monotonic per-session sequence
    numbers. RME-aware hosts may suppress duplicate legacy notifications without
    suppressing standard temperatures, `ok`, `busy`, resend, emergency, or
    pause/resume/cancel traffic.
  * Added a 30-second host-session lease renewed by `@RME SESSION KEEPALIVE`;
    expiry restores legacy notifications and disables remote UI input.
  * Unknown `@RME` commands return a protocol error without entering the G-code
    queue or disrupting OctoPrint's numbered-line state.
  * Removed a 640-byte automatic Marlin-stack snapshot from connection-time
    RME dispatch and reused the static transfer payload for text-bulk decoding.
    Durable resume now preserves its partial and metadata on transient reopen
    or re-hash failures, restores shared-monitor progress, and an invalid new
    SHA cannot erase an unrelated suspended upload.
    Selecting a different durable job likewise preserves the prior job's
    partial and metadata for later manifest-driven resume or explicit cleanup.
    Resume metadata is now fsynced through a private recoverable temporary
    record, and its formerly large path/record scratch state is static instead
    of consuming more than 800 bytes of Marlin task stack during BEGIN.
  * Long firmware-candidate verification periodically services thermal safety
    and the task watchdog while retaining streaming I/O and transfer speed.
  * RME actions now require complete tokens, crash-dump writes retain the
    shared transfer latch, and queued M32/M997 operations recheck the latch at
    execution so malformed lines or Connect/Link overtaking cannot start an
    unintended command or overlapping storage operation.
  * Moved complete bitmap font assets for MINI and all xBuddy targets into the
    signed managed resource image using compressed random-access glyph records
    and one shared cache. This preserves the complete UI/language feature set
    while restoring application-flash headroom.

Host implementers should use
[the protocol reference](doc/rme_serial_remote_protocol.md) and
[the handler integration guide](doc/rme_serial_handler_integration.md).

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
    * Loaded-filament overview and reassignment from the Filament menu without unloading or reloading
    * `M865 Q` serial query for loaded filament material reporting
    * Interactive filament loads and MMU preloads now request both material and color; successful unloads clear both assignments
    * MMU Loading Test now offers individual slots and Test All, with matching `M1704 P`, `K`, and `A` G-code selection
    * Added `M976` stationary loadcell calibration for per-print pressure advance and extrusion-health monitoring, including per-hotend temperature set/wait for slicer-driven tool and MMU sequences [C1, C1 INDX, XL, MK4, iX]
      * Added a guided Control-menu screen above Calibrations & Tests with dock-calibration-style loaded-tool toggles, an independent temperature for every loaded tool, one Run action, automatic material-profile temperatures, per-tool ±15 °C safety bounds, and a manual-only clean-area prompt
      * Manual and slicer-driven calibration use a whole-batch blocking heating/homing/probing/measuring/computing/cleanup progress screen visible over normal and serial prints; it takes over immediately, keeps chamber lights active, and permits only Abort until aggregated manual results are ready for Save to USB or Done
      * The blocking progress screen retains the normal temperature/status footer, and unattended batches wait for reachable pre-command nozzle targets so supplied MMU templates return to their explicit probing temperature before full MBL
      * MMU calibration explicitly unloads before every home/local probe and unloads the final slot before returning or presenting results, leaving the nozzle empty for the slicer's following full MBL
      * PA homes first, creates an absolute 10 mm minimum bed/nozzle clearance, then starts its 170 C probing preheat; it always cleans the nozzle before the local probe even when unload is skipped, waits for 170 C, and then heats to the requested test temperature
      * Shortened only the redundant slow-flow plateau and two coarse optimizer refinements, retaining four transitions cycles and three-point ±0.002 verification while reducing typical calibration filament use by about 25%
      * Moves the sheet down immediately after PA homing, preserves that idempotent clearance across later phases, and raises it to the probed anchor plus 3 mm only for off-bed extrusion; the post-probe transition now exits directly through the vent-safe anchor corridor without a generic-park reversal or redundant bed down/up cycle
      * Keeps PA-specific MMU load, unload, and free-air excitation at the same per-slot front service position at Y_MIN+1, with exactly 2 mm beyond the modeled nozzle path, so purge falls beyond the bed and is never carried across the sheet
      * Corrects aggregate PA noise normalization so strong 8-90 SNR transitions are no longer reported below one, unnecessarily retried, and rejected for the fallback
      * Filament-sensor sampling remains active during PA excitation, while runout/autoload event generation is scoped off until calibration cleanup completes to prevent false toolhead-sensor errors
      * Calibration temperatures are temporary; all previous hotend targets are restored after single, batch, cached, successful, or failed M976 invocations
      * Results are RAM-only and are recalibrated for each file or serial print
      * Slicer-provided physical-tool and logical-filament arguments support XL/MMU/INDX jobs and reuse cached results within the current job
      * One-command batch manifests preflight all material assignments and temperatures, then perform normal XL tool changes or reduced-prime MMU unload/load sequences for every used filament; the MMU calibration path primes only 2 mm beyond the modeled nozzle path to avoid a loose purge loop over the bed
      * Existing filament-profile `M572` pressure advance is the fallback, followed by a conservative material preset
      * Measurements below the configured confidence floor (75% by default) are retried up to a bounded safety limit and fall back instead of applying an unreliable result
      * A 300 ms stationary noise floor and derivative-free bracketed refinement replace the fixed sweep: measure the firmware material default, probe both directions at 0.020, shrink toward the best response, then always verify the winner and adjacent 0.002 values
      * Persistent minimum confidence (50–95%), minimum signal/noise (3–20), and confirmation retries (0–10) are configurable in the manual PA screen or with `M976 Q`, `N`, and `R`; `M976 Q` reports all acceptance settings and RME settings export/import preserves them
      * PA, filament load/unload, and every MMU command suspend pressure-based stuck/flow monitoring; nested operations cannot re-enable it early
      * After its final MMU unload, a batch moves the nozzle clear of the anchor before restoring or cooling to the previous target
      * Low-confidence PA measurements now complete with the safe profile/material fallback instead of emitting a serial `Error:` that cancels host prints; PA-related MMU unloads clean the nozzle on the front sacrificial strip before any cross-bed move
      * Fixed OctoPrint resend storms during long PA runs by marking fallback cache entries valid, using standard keepalives instead of per-step busy bursts, immediately acknowledging cosmetic numbered `M117`/`M73` host updates while M976 is active, and removing repetitive MMU communication-timeout reset diagnostics
      * Deduplicates unresolved MMU errors so the recovery UI and serial host receive one report, while changed errors and post-recovery recurrences are still reported
      * Added per-candidate `PA_CAL_DEBUG` telemetry for sample/transition validity, rejected low-signal or timing windows, capture overflow, amplitude, noise, SNR, transient score, and confidence
      * Replaced the SNR-3 confidence cliff with a continuous evidence score combining signal quality, transition coverage, repeatability, sample sufficiency, and winning-candidate separation; acceptance thresholds remain independent from the reported confidence
      * Fixed PA regressions by skipping MMU unloads only when working FINDA and extruder sensors both prove the path empty, parking and cleaning outside the printable area before real unloads, retaining the controlled 2 mm PA nozzle extension at the per-slot off-bed position, reusing valid homing state, and moving optimizer/debug storage off the nested M976 stack
      * INDX/purge-bin machines calibrate and clean over the bin; other supported machines extrude outside the printable boundary and finish in separate locally probed front-edge anchor slots below the first normal mesh-probe row
      * INDX PA excitation cycles run fast then slow and execute the blob-ejection wipe after each cycle, producing separate pellets; every successful drop increments the normal persistent waste-bin pellet count and participates in its configured pause threshold
      * Blocking M976 batches immediately report acceptance and emit phase keepalives; retransmission of the numbered command that is still executing is discarded with a busy response instead of triggering an RX flush and repeated forward-line requests
      * Continuous PrusaPATuner-derived 0.8/8.0 mm/s excitation is aligned from executed E-step positions and scored from transition error, overshoot and settling, with 0.002-second final PA resolution
      * PA results are reported to three decimal places and retain valid high-PA profiles up to the 0.500 safety ceiling
      * A conservative material volumetric-flow ceiling is applied instead of claiming a maximum from a short calibration ramp
      * The calibrated pressure response arms runtime detection for forward motion without pressure rise, drastic pressure collapse, and sustained high-flow pressure breakout; faults enter `M1601` stuck-filament pause/recovery
      * Sustained high pressure is now a soft max-flow marker rather than an immediate stuck-filament fault; a pause requires a subsequent pressure collapse or missing pressure rise, preventing thick healthy purge lines from triggering `M1601`
      * Runtime stuck-filament detection refreshes its idle baseline after settled layer travel, qualifies after 3 seconds/3 mm of continuous extrusion, and requires another 5 seconds/5 mm of bad pressure evidence; normal pressure, a pause, travel, or retraction resets the evidence
      * Stuck-filament recovery offers Continue, Unload, and Abort on-screen; paused serial hosts have the same direct `M1601 C`, `M1601 U`, and `M1601 A` actions
      * Connect reports Finished/Stopped only while the corresponding result screen is active and reports Idle from the home screen; aborts from nested recovery prompts retain normal cleanup and the stopped-result screen
      * Two hidden serial recovery slots remain active beyond the normal G-code queue, including during blocking heater waits; only recovery commands receive immediate handling, `M601` cancels the wait and pauses, and guarded `M602` resumes only from a safe stable pause
      * `M876 S<n>` selects a dialog button, `M876 A"<name>"` selects a canonical action independent of ordering or translation, and `M876 Q` reports queue usage, blocking command, pause/resume eligibility, and available actions; `M115` advertises all four service-protocol capabilities
      * Pressure-collapse evidence remains latched throughout that continuous bad-pressure window, preserving accurate grinding/tangle classification while the reference peak decays
      * Anchor occupancy is scoped to the active print job and resets with PA job state, preventing a cleared plate from cancelling the next serial print while preserving same-job collision protection
      * Front-edge PA travel starts to the right of the CORE One vent lever and uses an ordered safe approach; INDX retains dock-aware `mapi::park` waste-bin travel
      * Runtime pressure monitoring rearms from a fresh post-load baseline, derives expected pressure from the calibrated load step, and requires three seconds of continuous extrusion before a missing-pressure fault can qualify
      * Successful auto PA remains authoritative for the print even if later filament G-code supplies `M572`; that value is retained only as the fallback
    * Added the `coreone_indx` firmware image to the default `build.py` release matrix
    * Added configurable INDX purge-bucket pause thresholds and `M1986 Q/S/P/R` pellet counter control, including purge-bucket-full pause signaling to serial hosts
      * During a serial full-bucket pause, `M1986 R` resets a stale counter out of band while preserving the queued print commands; the print remains held until the host sends its normal `M602` resume
  * Fixes
    * Fixed MMU preload material selections remaining displayed as `Don't change` after entering the color picker
    * Added bordered color previews to normal-load and MMU-preload color selectors, including saved user colors; newly created colors now remain visible with their preview
    * Fixed normal MMU load completion emitting an unsolicited serial-host resume action, which could make OctoPrint change state mid-stream and enter a repeated numbered-line resend loop
    * Valid duplicate numbered serial commands are acknowledged without re-execution, allowing recovery when an `ok` is lost among asynchronous MMU or progress messages
    * RME builds now report the RME source repository in the local `M115` response
    * Fixed streamed-print completion resetting OctoPrint's numbered-command sequence by reserving `Done printing file` for actual media prints
    * Filament unload skips heating and motion only when all applicable healthy in-path sensors explicitly report empty: MMU ignores unrelated side state, INDX uses its one path sensor, and XL/MK use the selected tool path; disabled/failed sensors assume filament presence, and automatic runout still removes the remaining tail
    * PA calibration now emits the applied `M572` command and manual UI runs park the toolhead after results are handled
    * Fixed Prusa Connect chamber-light telemetry reporting 0% while an external GPIO chamber light is actually energized; the combined chamber-light state now reports 100%
    * Fixed filament runout during an existing print pause resuming the job after the filament-change sequence; the prior pause and print-timer state are now preserved
    * Stuck-filament recovery now offers Abort, clears its monitor latch, restores every displaced X/Y/Z print axis before resume, and does not emit host resume after abort
    * Known loaded material profiles are selected automatically for cancel-time unload temperature instead of prompting during print cancellation
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
    * Fixed Core One L release builds with bed PID disabled by using zero persisted bed-PID defaults when `PIDTEMPBED` is unavailable
    * Fixed XL release builds after the 6.6.1 typed-tool-index update by passing `PhysicalToolIndex` values to odometer APIs
    * Fixed XL dwarf release builds by using the Marlin serial error macro in the disabled-EEPROM path
    * Fixed the top-level release wrapper so managed `.venv` tools, including Nunavut `nnvg`, are automatically visible to CMake child builds
    * Fixed cached multi-version release rebuilds so stale nested CMake caches are rebuilt cleanly instead of leaving broken ExternalProject stamp state
    * Protected GPIO pins reserved for the external light bar from generic GPIO reconfiguration commands
    * Fixed Prusa Connect feature gating caused by the custom `-RME` firmware suffix; Connect registration, telemetry/events, and websocket requests now report the upstream-compatible firmware version
    * Fixed Prusa Connect chamber-light telemetry to report actual driven brightness; remote brightness values now act only as wake activity and do not overwrite RME lighting settings
    * Kept Japanese MINI release builds within flash with a compact material-only loaded-filament editor; color names remain visible and color metadata remains available through `M865` and settings import
    * Suppressed filament-runout handling while a print is already paused, parking, or resuming so toolhead maintenance during a pause does not trigger a second runout workflow

This is a custom firmware release based on upstream Prusa Firmware Buddy 6.6.2. It focuses on serial printing, OctoPrint usability, print-finished handling, LED behavior, chamber filtration, and external light bar control.

The release is based on upstream tag `v6.6.3` (`ff6658da4`) with the RME
changes carried on branch `rme-v6.6.3`. The RME port merge is `a59ee6f04`.

The running firmware UI uses the short `6.6.3-RME` version. Prusa Connect
intentionally receives the stock upstream `6.6.3` version string for
compatibility and feature gating, while local UI/API surfaces and release
artifacts keep the RME identity. The stock Prusa bootloader firmware-selection
menu still appends the mandatory BBF header build number.

## Upstream 6.6.3 base

This RME release includes all Buddy changes through upstream `v6.6.3`, plus
the complete RME stack previously released on 6.6.2. The inherited sections
below remain the authoritative detailed feature and behavior reference.

## Serial printing screen

A dedicated serial printing screen has been added for OctoPrint and other serial hosts. The screen shows print progress, printer status, filtered host messages, and a print-finished state with a `Continue` acknowledgement button.

Serial print start detection handles `M75`, eligible `M73 P0/Q0`, OctoPrint-style startup/status messages, and blocking heater waits such as `M109`, `M190`, and `M191`. Homing, mesh leveling, and ordinary toolhead movement no longer enter serial-print mode by themselves.

Starting a second serial print immediately after a previous print finishes now clears the previous finished/aborted state before entering the new serial print state.

Serial print completion is detected from explicit `M77`. Serial `M77` is handled before it enters the normal G-code queue, returns `ok` to the host, keeps the persistent finished screen active, and clears the serial command pause gate so later host commands and the next `M75` are accepted. Progress-only `M73 P100/Q100 R0/S0` reports are not treated as a hard end marker because they can arrive before streamed end G-code, including MMU unload commands, has finished.

Serial pause handling has been improved. `M601` from OctoPrint or another serial host now stops accepting additional streamed commands while the existing queue drains, then runs the normal print-head parking sequence. Serial commands are accepted again after the printer reaches the paused state so `M602` or host resume can be received.

For serial-host initiated `M601` and `M602`, the firmware no longer echoes new `//action:pause` or `//action:resume` requests back to the host that sent the command. It still reports `//action:paused firmware_pause` after the head is actually parked and `//action:resumed` after reheating/unparking has returned the printer to the printing state. This keeps OctoPrint state synchronized without recursively invoking OctoPrint pause/resume scripts or host-side heater-off actions.

For screen- or firmware-initiated pause/resume during a serial print, the firmware still sends the matching `//action:pause` or `//action:resume` request before the transition and the final `//action:paused` or `//action:resumed` confirmation after the printer completes the transition, so OctoPrint can run its after-pause and after-resume scripts.

Firmware pause states, MMU errors, and runout-style pauses keep the print treated as active for the print screen and chamber lighting. They also hold the bed-heater safety timer reset so the bed does not cool during a recoverable paused print. The printer reports pause/resume style host actions back to serial hosts where applicable.

MMU filament runout and other manual-intervention recovery on serial prints can restore the nozzle target before resume when needed and now send the serial-host resume action after the printer resumes, so hosts such as OctoPrint do not remain paused after the printer has recovered.

Print-time runout now also watches the toolhead filament sensor when an MMU, side, or external filament sensor is the primary runout source. This lets filament breaks or jams between the upstream sensor and the extruder trigger the normal M600 pause instead of relying only on the upstream sensor.

Filament runout is now ignored while the print is already paused, parking, or resuming. This avoids triggering a second runout recovery flow while the user is intentionally working around the toolhead during a pause.

Loadcell stuck-filament detection remains configurable from `Stuck Filament Detection` and `M591 S`. A separate `Filament Movement Detection` setting and `M591 U` control the new movement/tangle protection, while both settings share the same loadcell E-stall detector and M1601 pause path. `M591 I/F` debounce tuning remains available for skipped extrusion detection.

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

## Loaded filament management

The Filament menu now includes a `Loaded Filament(s)` entry. On single-tool printers it opens the current loaded filament assignment directly; on MMU and multi-tool printers it lists each tool so the loaded material can be reviewed per slot.

Loaded filament records now also persist color. The management screen offers the standard palette and eight user-defined slots. Load/change operations retain their requested color, `M865 Q` reports the color name and `#RRGGBB` alongside material, and custom definitions are included in RME settings export/import.

Selecting a loaded filament opens the material picker and updates the stored material assignment without running a load, unload, purge, or tool-change workflow. This is intended for correcting the printer's material metadata after a manual swap, toolhead service, or host-driven workflow where the filament is already physically loaded.

Serial hosts can query the loaded filament assignments with:

```gcode
M865 Q
```

The response reports one line per loaded tool in the form `loaded_filament T0 S"PLA"`. This gives OctoPrint and other serial hosts a stable way to discover the printer-side loaded material state for future integration work.

Color reassignment uses the existing filament-material profile metadata. There is not a separate persistent loaded-color slot in this release, so color reporting follows the selected material profile rather than storing an independent spool color.

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

During first boot after flashing, bootstrap/install screens hold the configured Active screen brightness until resource, bootloader, ESP, and puppy setup is complete. The normal Active-state 15% readability floor still applies. The firmware paints the splash screen before starting those install steps, and the post-boot idle timer is seeded only after the first home-screen paint so the printer cannot dim immediately based on pre-display boot time.

Every application startup now begins with a bounded 30-second Active lighting window. This also covers startup into the retained-crash export/error screen, so an Idle or Deep Idle saved profile cannot leave the display dark while the printer is booting or saving a crash dump. Printing still takes precedence, real user/host activity starts the normal configured activity timer, and the startup window expires automatically so it cannot hold the machine Active indefinitely.

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

Loaded-filament management is consolidated in the root Filament menu. Tool
rows show material and color names with theme-safe bordered color swatches; Material and Color changes are staged and
committed together with Save. Empty user-color slots stay hidden, new named
colors are created under Settings > Filament Colors, and exact hex values are
reserved for the custom-color editor and `M865` serial reports. PA calibration manifests and the manual selector now cover all eight INDX tools, with an eight-tool batch example in the M976 guide.

Submenu arrows now use the resolved menu text color instead of the theme image
accent, preserving contrast for Indigo and custom themes in both focused and
unfocused list rows.

Settings > FW update now opens a firmware-update page with USB BBF selection
and the existing update instructions. The picker uses a dedicated firmware
filter so `.bbf` files are visible instead of being removed by the printable
G-code filter. Selecting a root-level 8.3 `.bbf` asks
for confirmation, records the selected file for the bootloader, and restarts.
The bootloader performs its normal signature, printer-model, and compatibility
checks before writing firmware.

Serial hosts can request the same bootloader handoff with
`M997 /usb/FIRMWARE.BBF`, provided the BBF already exists in the USB root and
uses an 8.3 short filename. `M997 O` retains the existing automatic/forced
restart update behavior. Stock firmware has no binary BBF upload transport on
the normal serial G-code channel.

RME now adds that binary-safe transport as `M998`. A host begins with the
declared byte count and SHA-256 digest, sends strictly sequential Base64 chunks
of at most 48 decoded bytes, and finalizes only when both size and hash match.
The firmware writes `/usb/FWUPD.TMP` and renames it to `/usb/FWUPD.BBF` only
after verification. Upload and flashing remain separate operations; the host
must explicitly send `M997 /usb/FWUPD.BBF` afterward. See `doc/gcode/M998.md`.

MINI exposes the same guided firmware picker and complete `M998` staging
protocol as the MK and CORE families. To retain feature parity with adequate
internal-flash headroom, all three complete MINI bitmap fonts are packaged as
independently compressed random-access records in the signed external resource
image and glyph-cached on demand. No characters,
languages, update controls, or upload validation paths are removed.

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

`--final` builds retain the `-RME` full and short version suffixes while removing numeric development suffixes from the running application firmware version. The installed firmware UI identifies the custom firmware as `6.6.2-RME`. The stock Prusa bootloader firmware-selection screen still appends the mandatory BBF header build number after the `RME` tag; removing that bootloader-rendered number requires a bootloader change. Final builds disable `DEVELOPMENT_ITEMS` by default unless explicitly overridden. Final/non-development builds keep the full `M503` report command enabled, including human-readable headings/comments and TMC settings. Replayable persistent settings remain reportable, including PID, motion, probe, and other saved configuration values. PID editing, autotune, save, and load commands remain available through `M301`, `M303`, `M500`, and `M501`. Core One, Core One L, XL, and MINI store translations in the external resource image. The Core One/Core One L/XL resource image size is large enough to fit the web assets, puppy firmware, QOI assets, MMU firmware where applicable, and translation files.

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

The firmware-update picker now accepts completed `.bbf` images anywhere on the
USB drive, including long or nested paths chosen by Prusa Connect. It preserves
the downloaded source while copying the selected image to the bootloader-safe
`/usb/FWUPD.BBF` path, flushes the staged file, and only then restarts for the
normal signature and printer-model validation. A UI-only marker removes the
temporary short-name copy when the application next mounts the USB after the
bootloader attempt, preventing stale firmware from remaining selectable while
leaving the original Connect download intact.

The picker now keeps its 1 KiB copy workspace out of the constrained GUI task
stack and delegates restart to the established `M997 /usb/FWUPD.BBF` command
path. This prevents a stack-induced frozen update screen and performs the
retained bootloader request and reset from the same context as serial updates.

Release builds can optionally sign generated `.bbf` files by setting `FIRMWARE_SIGNING_KEY` when running the top-level release wrapper:

```sh
FIRMWARE_SIGNING_KEY=/path/to/private.key ./build.py
```

If `FIRMWARE_SIGNING_KEY` is not set, `./build.py` uses the machine-local default key at `.local/firmware-signing-key.pem` when it exists.

The underlying build wrapper also supports `--signing-key /path/to/private.key`.

The top-level `./build.py` wrapper defaults to at most four concurrent printer builds to avoid overwhelming the build machine. Use `--jobs N` to override the cap when appropriate. A shared `.dependencies/.rme-build.lock` prevents simultaneous wrapper/version-worktree builds from corrupting common build or BBF outputs; multi-version children inherit the parent lock. Ctrl-C, normal exit, a crash, or reboot releases the kernel-held lock automatically even though the harmless lock file remains. Interrupted wrapper builds terminate active child processes instead of leaving orphaned Ninja/LTO jobs running. Completed builds report each machine's flash usage, aggregate RAM usage, individual memory-region usage, total elapsed wall-clock time, and absolute staged BBF path. The wrapper also prepends `.venv/bin` to child build `PATH`, sets `BUDDY_PYTHON`, and passes `Python3_ROOT_DIR` and `Python3_EXECUTABLE` by default so managed virtualenv tools such as Nunavut `nnvg` are available during CMake configuration and nested puppy-firmware builds.

The wrapper can also build multiple maintained RME release branches in one command:

```sh
./build.py --final --jobs 14 --versions 6.5.7 6.6.2
```

This checks out each requested `rme-vX.Y.Z` branch through a cached Git worktree under a sibling `.Prusa-Firmware-Buddy-rme-version-builds/` directory, runs that version's normal release wrapper, and stages artifacts under `bbf/X.Y.Z/`. Keeping the version worktrees outside the source checkout avoids accidental relative include leakage while still allowing each cached worktree to retain its `build/`, `.venv/`, and `.dependencies/` directories so future rebuilds of the same versions can reuse CMake, compiler, Python, and downloaded dependency state instead of starting cold. If a nested ExternalProject cache was configured with the wrong Python, or if an old nested cache was partially removed while CMake stamp files remained, the wrapper removes the affected preset build directory so the next rebuild starts from a coherent CMake state.

The wrapper can also build multiple maintained RME release branches in one command:

```sh
./build.py --final --jobs 14 --versions 6.5.7 6.6.2
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

The complete RME patch stack on upstream `v6.6.3` was checked with the final
multi-version release build. The wrapper supplies the managed `.venv` path,
`BUDDY_PYTHON`, `Python3_ROOT_DIR`, and `Python3_EXECUTABLE` automatically so
nested CMake projects use the same managed Python environment as the parent.

Final firmware validation at `8a9b632ef` passed with:

```sh
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

The complete command produced all `29` expected BBFs with no stray files in
`bbf/`. The `6.6.3` half passed all `15` release presets, covering CORE One,
CORE One INDX, CORE One L, MINI language variants, MK3.5, MK4, and XL. This
documentation refresh does not change the validated firmware output.

Key final flash measurements were: MK4 59.99%, MK3.5 55.36%, CORE One
63.96%, CORE One INDX 64.73%, CORE One L 64.00%, XL 68.15%, and the largest
MINI language image 97.57%.

Earlier validation for this patch stack on the prior upstream base covered the host `mmu_tests` unit-test target with `110041` assertions in `4` test cases. The signing path was validated with a temporary ECDSA key:

```sh
python3 utils/build.py --preset coreone --bootloader yes --skip-bootstrap --no-store-output --signing-key /path/to/private.key
```

That build produced a non-zero BBF signature.

## Changelog base

Comparison base: upstream `v6.6.3` (`ff6658da4`)

Current branch: `rme-v6.6.3`

Latest firmware commit: `8a9b632ef`

Release-documentation commit: `05f0ce7eb`

Expanded release-notes commit: `3c75548b7`

Published release tag: `v6.6.3-RME-b3`

Port-refresh commits: `Finalize 6.5.7 RME release port`, `Fix Prusa Connect serial print state reporting`, `Fix serial MMU print completion unload`, `Port RME firmware to Buddy 6.6.0`, `Fix serial M601 M602 host actions`, `Restore previous screen after ignored serial macro`, `Release RME firmware 6.6.1`, `Fix RME release build environment`, `Keep toolhead runout active with upstream sensors`, `Update 6.6.1 RME release notes`, `Split filament movement detection control`, `Add cached multi-version RME release builds`, `Suppress filament runout while paused`, `Add loaded filament reassignment and query`, `Add per-print extrusion calibration`, `Improve PA tuning and monitor extrusion pressure`, `Add batch PA calibration orchestration`, `Add guided manual PA calibration`, `Add persistent loaded filament colors`, and `Refine PA material safety and Connect light handling`

The port-refresh commits cover the upstream status LED state merge, the 256-field generated config-store visitor needed by XL, Prusa Connect serial-print state parity, serial MMU print-completion unload behavior, Buddy 6.6.0 API/resource compatibility fixes, serial M601/M602 host-action synchronization, ignored short serial macro finished-screen restoration, the upstream 6.6.2 translation refresh, release build environment fixes, the secondary toolhead runout path for MMU/side/external filament sensor setups, split filament movement detection control, cached multi-version RME release builds, paused-runout suppression, and loaded filament reassignment/query support.

## Full changelog

Historical RME commits inherited from the 6.6.2 release:

```text
b2a919d4ea  2026-05-16  new octoprint screen and better color scheme
b603e403cc  2026-05-16  transfer features for previous patch set (not company specific)
fb9374c067  2026-05-16  Ignore crckill build output
4a0fa5d88b  2026-05-16  Fix terminal dirty flag buffer clearing
e2596a1d9a  2026-05-16  screen fixes
2762e98a38  2026-05-16  octoprint usability improvements
d5bbfd4435  2026-05-17  add better led control to the printer
227a62fa1f  2026-05-17  serial print overhaul
5fae8c0112  2026-05-17  fix possible bug
887a9c7069  2026-05-17  better chamber fan controls.
61c43d7d7d  2026-05-17  support other machines to
ad3f9623e5  2026-05-17  fix print start and temp readings
7ae79ca417  2026-05-17  version fixes
b772c6cc6f  2026-05-17  massive ui overhaul
f2276962fb  2026-05-17  better detection
926c1444d9  2026-05-17  better serial print screen
ed7e3afb3d  2026-05-17  better reliability
f8f949ec0e  2026-05-17  massive theme overhaul
16a7bdd948  2026-05-17  added theme validation for file import
4fb3a9523c  2026-05-17  add build alias
a3c67bcea7  2026-05-18  theme fixes
b63745160b  2026-05-18  fix some serial print issues
df84a77d78  2026-05-18  better build script
9c57b27975  2026-05-23  better led control
3ab6764ab5  2026-05-23  fix serial percent parsing
e5f1a6595c  2026-05-23  round up to the nearest percentage
3f37d08d70  2026-05-24  more changes to les statuses and other features
d7ef26b823  2026-05-24  led fixes for non core one printers
28d10b1b96  2026-05-24  fix mini builds
8a074503eb  2026-05-25  external led fixes for core one variants and for mk variants
ea8aec8a2f  2026-05-25  attempt to reduce first boot flicker
5b25960715  2026-05-25  some other misc fixes for led and print state
ab7005e14e  2026-05-25  some other misc fixes
4dfc2d8353  2026-05-25  even more misc fixes
0e5350bb1d  2026-05-25  even more misc fixes
a81bd6c725  2026-05-25  led config overhaul
ff373a100d  2026-05-26  code cleanup
923cad87d3  2026-05-26  code cleanup
bb5e35db65  2026-05-26  some fixes
10ce278d07  2026-05-26  more fixes
4f00efc6b5  2026-05-26  more fixes
f7998a25ba  2026-05-26  more fixes
68b46c6a49  2026-05-26  more fixes
c8084881d6  2026-05-26  better ui
875720ec3f  2026-05-26  mini build fixes
f10b5bc7c4  2026-05-26  mini build fixes
251a475614  2026-05-26  lots of bug fixes
29f714f748  2026-05-26  more fixes and renaming
77a0590cdc  2026-05-26  xl fixes
e6b7b375c6  2026-05-27  size improvments and other fixes
592a3d855f  2026-05-27  more fixes and added a playbook for future use
427aead6de  2026-05-27  external lighting fixes
1d364169ad  2026-05-27  build fix for mk3.5
b7874aed1d  2026-05-27  playbook update
04a0ac3d56  2026-05-27  lower flash usage
60aa33e2af  2026-05-27  update playbook
04e8f85855  2026-05-28  updates for better leds and screen brightness control
efe53d365b  2026-05-28  update playbook
15d3408810  2026-05-28  better screen brightness controls
5361f09d2a  2026-05-28  update the playbook
2e282b04c6  2026-05-28  massive screen fixes
133d8d8e68  2026-05-28  more fixes
f92ac521d9  2026-05-28  fxi print screen and added support for saving custom pid c=values / changing them in the ui and restoring the factory defaults.
b0c022d755  2026-05-28  better flash managments and build script fizes
834054cf73  2026-05-28  playbook update
85232ad273  2026-05-28  fix layout stuff
c74a5a7e68  2026-05-28  fix print progress display
1cc02c4311  2026-05-28  add auto pid process
601d2752d7  2026-05-28  add auto pid process
d1a651070a  2026-05-30  fixed print screen issues
8c1913578f  2026-05-31  updated readme
5d31cf6abe  2026-06-01  updated a lot of stuff for better filter control and more
c9d225be91  2026-06-01  led fixes for all systems
4ebd89f74d  2026-06-01  better m503 for release builds
86dfd75812  2026-06-01  build script overhaul
cdb5a42306  2026-06-01  build script overhaul
1a43c25a34  2026-06-01  fxi the rme suffix
5315f6b254  2026-06-01  fix issues and core one vent behaviour as well as fixed the z move crash
7ed2597f9f  2026-06-01  fix issues and core one vent behaviour as well as fixed the z move crash
f19989e42e  2026-06-01  fixes for the printing screen
decb7b0f89  2026-06-01  fix filter usage tracking
7b6477c6dc  2026-06-01  Gate filtration lighting until print completion
fd5dffe359  2026-06-01  Preserve print duration and restore status brightness cycling
f96ac42583  2026-06-01  Apply theme status colors and smooth filtration pulse
2b2a654546  2026-06-01  Migrate Indigo status LEDs away from cyan
8532e9e347  2026-06-01  Keep Indigo LED color profile driven
4507e107d2  2026-06-01  Keep non-Indigo printing LEDs cyan
e8e754fb8e  2026-06-01  Use saturated Indigo for status LEDs
ac77ba7a0c  2026-06-01  Darken Indigo status LED profile
7c0b41bdaa  2026-06-01  Restore stable Indigo LED calibration
02a95bf581  2026-06-01  Stabilize Core One RGBW LED PWM
a44da6d207  2026-06-01  Linearize Core One RGBW strip PWM
46c2230f84  2026-06-01  Make solid LED calibration overrides immediate
8201681c5a  2026-06-01  Preserve M150 overrides until state changes
0cc7555d74  2026-06-01  Restore reliable cyan printing status bar
3c7b6763f1  2026-06-02  Fix RGBW lightbar color scaling and restore Indigo profile
8d77523013  2026-06-02  Fix serial print completion and refine status lighting
d95e7688e0  2026-06-02  Document serial completion and filtration lighting updates
14f0d0278a  2026-06-09  Add Connect compatibility reporting and heater safety menu
6858ab6d3c  2026-06-09  Document Connect and heater safety updates
4d93fd0b5c  2026-06-09  Split PID settings by heater
4ff6b10236  2026-06-09  Show filtration time in print finished stats
b62ca7a7b8  2026-06-09  Refresh release playbook audit numbers
7649563c41  2026-06-10  Fix print lighting brightness overrides
00e98fcc2d  2026-06-10  Fix print lighting overrides and completion state
dc3cd187d8  2026-06-10  Document print lighting override behavior
61823cdb0b  2026-06-10  Update release notes for lighting override fixes
89fe0e79a0  2026-06-10  Fix serial print finish and intervention handling
b8c65581b5  2026-06-10  Document serial finish and intervention fixes
ab231bdc2a  2026-06-10  Defer MMU serial host actions to server loop
748ac1bd9f  2026-06-10  Document MMU serial host crash fix
8d624a232b  2026-06-10  Fix OctoPrint uploads and add filtration trigger G-code
cae8263ff6  2026-06-10  Document filtration trigger and upload fix
324fc25f7c  2026-06-10  Add MMU serial host event unit coverage
7f8d08e000  2026-06-10  Document MMU unit coverage validation
628fdfc67e  2026-06-24  Finalize 6.5.7 RME release port
2ff86e23fd  2026-06-24  Fix Prusa Connect serial print state reporting
f6b1448c02  2026-06-25  Fix serial MMU print completion unload
ce7f68409a  2026-06-25  Port RME firmware to Buddy 6.6.0
63a62eead6  2026-06-25  Update 6.6.0 RME release documentation
d1df63b01a  2026-06-25  Port 6.5.7 parity fixes to 6.6.0
2834ddc48b  2026-06-30  Fix serial M601 M602 host actions
4e44c70d40  2026-07-02  Restore previous screen after ignored serial macro
79618c56c6  2026-07-02  Release RME firmware 6.6.1
6be03b005a  2026-07-03  Fix RME release build environment
899a4eaa03  2026-07-06  Keep toolhead runout active with upstream sensors
368b614d2  2026-07-06  Update 6.6.1 RME release notes
718c3c493  2026-07-06  Split filament movement detection control
25eb605d3  2026-07-07  Add cached multi-version RME release builds
790311418  2026-07-10  Suppress filament runout while paused
08e862da0  2026-07-11  Add loaded filament reassignment and query
763efb32d  2026-07-11  Update 6.6.2 RME release notes
5d3804260  2026-07-11  Rewrite 6.6.2 RME release notes
c493d7be2  2026-07-18  Add per-print extrusion calibration
7b8b49471  2026-07-18  Improve PA tuning and monitor extrusion pressure
539fb8c14  2026-07-18  Add batch PA calibration orchestration
f7619812b  2026-07-19  Update PA calibration commit reference
681e639f3  2026-07-19  Document slicer setup for auto PA
6199d79c0  2026-07-19  Preserve paused runout state and manage INDX wastebin
4a56a1276  2026-07-19  Allow serial reset during INDX bucket pause
02b5dd8c2  2026-07-19  Update 6.6.2 release commit ledger
b9bfcaf12  2026-07-19  Merge upstream Prusa firmware 6.6.2
59a627a41  2026-07-19  Add guided manual PA calibration
e5546b120  2026-07-19  Add persistent loaded filament colors
ad3077371  2026-07-19  Fix persistent custom color name lifetime
0f34e6226  2026-07-19  Refine PA material safety and Connect light handling
85fd307cc  2026-07-19  Keep MINI color UI within flash budget
962a5257b  2026-07-19  Add guided PA calibration progress workflow
9fb33d48f  2026-07-19  Polish PA probing and chamber light telemetry
7224ae999  2026-07-19  Allow cold-start PA probing sequence
b8cff9a9e  2026-07-19  Guard PA Connect state on loadcell builds
```

### Final release ledger continuation

The branch-specific commits added after the original ledger was generated are:

```text
b6429c0de  2026-07-19  Report firmware print states to serial hosts
883d109ed  2026-07-19  Update serial status commit ledger
26f8abbe7  2026-07-19  Update 6.6.2 serial status ledger
9403d03e3  2026-07-19  Fit serial status reporting on MINI
51763a896  2026-07-19  Reuse status page for serial initialization
be89cfc6f  2026-07-19  Deduplicate MINI tune settings for flash
e2d3b26d5  2026-07-20  Fix manual PA calibration workflow
4e93def0c  2026-07-20  Suppress filament sensor events during PA test
be01f32b0  2026-07-20  Serialize release builds
04da073d7  2026-07-20  Redesign loaded filament editor
c4aeb83bf  2026-07-20  Fix virtual tool color lookup
638f18d79  2026-07-20  Fit filament editor within MINI flash
07efc05b3  2026-07-20  Reduce loaded filament menu footprint
60180c9a3  2026-07-20  Respect MINI firmware size budget
4d271432e  2026-07-20  Keep MINI loaded filament UI compact
4345354aa  2026-07-20  Apply MINI filament selection directly
5399b625b  2026-07-20  Document loaded filament menu layout
6c90b75c9  2026-07-20  Improve filament color and menu visibility
790be450e  2026-07-20  Add USB firmware update picker
75514d8a9  2026-07-20  Reduce firmware picker footprint
1fc3a27fb  2026-07-20  Fit firmware picker on MINI
13202f2d0  2026-07-20  Add complete serial firmware upload and PA controls
406db1faf  2026-07-20  Compress external MINI fonts per glyph
593b04aef  2026-07-20  Finalize compressed MINI resources and upload decoder
c0509f2a0  2026-07-20  Show filament color swatches and support eight-tool PA
70e3f2cf8  2026-07-20  Use print temperatures and reduce PA MMU purge
084b1cf3e  2026-07-20  Flash Prusa Connect firmware downloads from UI
244cae371  2026-07-20  Clean up UI firmware staging after update
3b964a5c6  2026-07-20  Keep MMU unloaded for PA probing and mesh
050cabfc2  2026-07-20  Eject and count INDX PA pellets per cycle
a070dd8fe  2026-07-20  Prevent serial resend storms during M976
9d923e440  2026-07-20  Fix pressure advance calibration footer
aa3c5ccdf  2026-07-20  Fix serial print completion and PA handoff
5dd32b013  2026-07-20  Adapt PA final park to current motion API
a1b5194aa  2026-07-20  Skip unload only for confirmed empty paths
c06f69073  2026-07-20  Fix MMU serial flow and filament workflows
8aabadbc5  2026-07-20  Update RME release playbook for MMU workflows
113b4e7ae  2026-07-20  Fix filament color selection previews
e0b2a7578  2026-07-20  Harden PA calibration and unload monitoring
54e714245  2026-07-20  Optimize PA confidence calibration
6b04989a5  2026-07-20  Use generated loadcell option in MMU guard
326b018ee  2026-07-20  Keep PA fallback nonfatal and clean MMU purge
bdd1de5c1  2026-07-20  Prevent serial queue overflow during PA calibration
a97b977e4  2026-07-20  Add pressure advance measurement diagnostics
711c0e4fa  2026-07-20  Use continuous PA calibration confidence
84d2a66fb  2026-07-20  Fix MMU PA preparation and stack crash
f31bdc5bc  2026-07-20  Fix MMU PA sensor namespace
44e512ee9  2026-07-20  Fix firmware picker and PA probe preparation
42ec3692d  2026-07-21  Reduce PA calibration filament use
ecfb81ca4  2026-07-21  Keep PA off bed and fix signal confidence
ea79004d5  2026-07-21  Add pressure advance debug setting
039e8b665  2026-07-21  Fix firmware picker reboot handoff
cad09185b  2026-07-21  Fix PA vent travel and extrusion fault recovery
8a15550ec  2026-07-21  Rearm pressure monitor after filament handling
5b5034cf0  2026-07-21  Include pressure monitor API in stuck recovery
4c3969d0b  2026-07-21  Document PA travel and fault recovery safeguards
df6788adc  2026-07-21  Fix PA MMU purge placement and pressure faults
37d8ebd89  2026-07-21  Debounce runtime extrusion pressure faults
9b71400a0  2026-07-21  Audit RME release documentation for 6.6.2
0da1735be  2026-07-25  Scope PA anchors to the active print job
e31243f9d  2026-07-25  Smooth pressure advance service motion
2d73c9e8a  2026-07-25  Deduplicate unresolved MMU errors
c125d4ad1  2026-07-25  Harden stuck filament recovery
d6c53de51  2026-07-25  Fix 6.6 stuck response phase include
b7184c726  2026-07-25  Guard stuck response on filament sensor
2ceab3e72  2026-07-25  Align Connect state with print result UI
e38085f33  2026-07-25  Reserve serial recovery command capacity
be6fccbc3  2026-07-25  Keep serial recovery reserve always active
87b077038  2026-07-25  Expand serial recovery service protocol
```

This inherited continuation includes firmware changes through `87b077038`.
The 6.6.3-specific continuation is:

```text
2871cf5de  2026-08-08  Add out-of-band serial remote control
222fcefd4  2026-08-08  Fix remote filament names on 6.5 firmware
dae2b9f97  2026-08-08  Document and extend RME remote control
48644bef2  2026-08-08  Support machine discovery on 6.5.7
206bb50e6  2026-08-08  Adapt INDX LEDs to RME print states
d9ad3ce2f  2026-08-08  Reduce remote protocol flash overhead
9c5c98f02  2026-08-08  Fit remote control protocol on MK4 targets
4993107e1  2026-08-08  Record 6.6.3 release matrix validation
ba276459e  2026-08-08  Expand out-of-band recovery workflows
c3b56a703  2026-08-08  Report unknown RME subcommands safely
7d8d54980  2026-08-08  Keep recovery protocol within MK4 flash
3fdeda3bc  2026-08-09  Move xBuddy fonts to managed resources
daaf8c17e  2026-08-09  Record xBuddy resource build results
58177879a  2026-08-09  Add structured RME host workflows
9e76ac329  2026-08-09  Expire inactive RME host sessions
87806d4d4  2026-08-09  Record 6.6.3 RME release validation
3c75548b7  2026-08-09  Expand 6.6.3 RME release notes
4569d3cec  2026-08-09  Refresh 6.6.3 RME release commit ledger
9a99ddbba  2026-08-09  Expand RME device workflow events
5e3742e25  2026-08-09  Expose RME printer statistics
740910ac2  2026-08-09  Make RME stats portable across releases
b774b3c1d  2026-08-09  Persist release build failure logs
4feb82edf  2026-08-09  Document expanded RME protocol workflows
6544c2c1b  2026-08-09  Fix RME session activity state
adde7a9bf  2026-08-09  Expand RME filament profile synchronization
8a9b632ef  2026-08-09  Add RME USB filesystem service
05f0ce7eb  2026-08-09  Document RME filesystem release validation
```

`v6.6.3-RME-b3` identifies this release ledger. Release documentation is
expanded through `05f0ce7eb`. The exact 15-image 6.6.3 half of the final multi-version
matrix passed at firmware head `8a9b632ef`; MK4 used 59.99% flash and MINI used
at most 97.57%.

## Build 4 release update

Build 4 completes the high-speed host, manufacturer, configuration-stream, and
INDX tool-model work while retaining the complete build 3 feature set:

* load, preload, and loaded-filament workflows expose 50 built-in manufacturers
  plus eight persistent keyboard/RME-defined user entries;
* raw binary and pipelined RME transfers coexist with legacy commands and baud
  fallbacks;
* local and host-originated configuration changes emit sequenced `RME_CHANGE`
  records, allowing one startup/reconnect snapshot with no steady-state polling;
* INDX reports eight real tool positions, never Marlin's internal ninth
  `NoTool` sentinel, and dock setup explicitly selects the four- or eight-tool
  hardware variant while clearing stale tools outside the selected variant;
* managed external resources continue to hold immutable QOI assets,
  translations, and supported fonts; manufacturer strings are packed once and
  multi-filament menu operations no longer duplicate their bodies per tool; and
* firmware flashing text was moved into a dedicated splash status region; build
  5 corrects that region to below the bar so it cannot overlap boot graphics.

The final release command was:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

All 15 6.6.3 presets passed. Maximum MINI flash use was 98.47% (Japanese
language image), MK4 used 60.65%, MK3.5 used 56.02%, XL used 68.79%, CORE One
INDX used 65.37%, and all other CORE variants remained below 64.67%. The staged
release directory contains exactly 15 BBFs under `bbf/6.6.3`.

Build 4 firmware continuation:

```text
9c32cb071  2026-08-09  Update 6.6.3 RME release commit ledger
12f9c7d66  2026-08-09  Prepare 6.6.3 RME build 3 release
fe55ecccf  2026-08-09  Add loaded filament manufacturer tracking
679e215e9  2026-08-09  Advertise faster compatible USB serial mode
e9e0b7af0  2026-08-09  Advertise RME serial rate fallbacks
f0d39414f  2026-08-09  Show RME host and transfer status
4e746daa9  2026-08-09  Add themed RME host badge
36a2311d5  2026-08-09  Add negotiated RME bulk file transfer
347690a53  2026-08-09  Add raw RME binary uploads
dcbee127d  2026-08-09  Keep binary uploads outside queue backpressure
9fb5784ca  2026-08-09  Document high-speed RME transfer protocol
65257803b  2026-08-09  Remove duplicate binary transfer summary
df359c20d  2026-08-09  Push RME configuration changes to hosts
ec886e1ea  2026-08-09  Support manufacturer metadata for every INDX tool
115939edf  2026-08-09  Correct INDX tool variant reporting
8759cf266  2026-08-09  Reduce filament UI flash usage
```

Published release tag: `v6.6.3-RME-b4`.

Release-documentation commit: `2ac3b5d6b`.

## Build 5 release update

Build 5 fixes firmware-update handoff, host synchronization, lighting idle
behavior, and filesystem replacement without reducing the build 4 feature set:

* firmware staged as `/usb/FWUPD.BBF` is armed as a durable one-shot update and
  removed after the requested bootloader attempt even when USB is already
  mounted, preventing a second flash and double boot;
* `M997` drains its acknowledgement and structured reconnect marker, explicitly
  detaches USB CDC, and then resets so OctoPrint can observe clean removal and
  re-enumeration;
* the installation status is below the progress bar in the application-owned
  status area, preserving the complete Prusa logo and hiding stale bootloader
  text;
* loaded-filament changes notify RME hosts only after material, color, and
  manufacturer are committed, eliminating stale loadout snapshots;
* every `@RME` service frame is passive for the local activity timer, and a
  display wake over a dim/off print override expires after 30 seconds so the
  screen and chamber lighting can turn off normally; and
* legacy, bulk, and binary RME uploads advertise `overwrite=1` and replace an
  existing regular file through verified sibling staging with rollback;
* RME downloads now negotiate CRC32-protected raw 1024-byte reads, with a
  bounded frame per request and the original 48-byte Base64 mode retained for
  older hosts;
* RME-owned uploads and downloads retain the indigo R in the header alongside
  transfer progress instead of replacing it with the generic transfer icon;
  and
* host transaction IDs are parsed over their full unsigned 32-bit range, so
  large IDs no longer saturate at `2147483647` during profile synchronization.
* RME USB CDC uses bounded 512-byte RX/TX FIFOs with streaming backpressure,
  recovering 7 KiB of SRAM that previously could exhaust the heap during
  connection, ordinary G-code motion, or file operations;
* binary upload and download share one 1024-byte payload buffer and stream each
  verified chunk directly to or from disk, recovering another 7 KiB of SRAM
  without materially reducing USB throughput;
* PA load-cell capture retains more than 30% margin over measured runs while
  returning 9 KiB of its always-resident sample storage to the system heap;
* legacy `M998` firmware upload keeps one sequentially buffered staging file
  open instead of allocating and releasing stdio/FatFS state for every 48-byte
  chunk, and all RME read/export/marker streams disable redundant lazy stdio
  buffers;
* firmware staging uses the neutral `FWUPD.RME` filename, and its cleanup
  marker is created only together with the explicit M997 reboot request, so a
  partial or unselected upload cannot flash on an unrelated boot.
* transfer recovery, encryption metadata, HTTP and inline engines,
  partial-file ownership, and both 4 KiB USB DMA sector buffers use bounded
  fixed storage; transfer metadata and partial-file streams are explicitly
  unbuffered. RME connection, staged-file recovery, and upload/download startup
  therefore no longer depend on runtime heap headroom.
* Connect command parsing now retains its existing fixed shared buffer with an
  intrusive borrow count instead of allocating `shared_ptr` control blocks for
  G-code, paths, tokens, and hostnames, removing the remaining normal
  connection/command synchronization heap churn.
* RME parameter, transaction, manufacturer, filesystem-path, digest, and frame
  parsing now share one allocation-free implementation covered by host unit
  tests. The tests preserve full-width transaction IDs, reject negative or
  overflowing unsigned fields, reject truncated percent escapes and fixed
  buffer overflow, contain paths below `/usb`, isolate service frames from
  ordinary G-code, and stress 100,000 repeated synchronization commands.

The final release command was:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

All 15 6.6.3 presets passed. Maximum MINI flash use was 98.64%, MK4 used
60.71%, MK3.5 used 56.08%, XL used 68.84%, and CORE One INDX used 65.43%.
The staged release directory contains exactly 15 BBFs under `bbf/6.6.3`.

Build 5 firmware continuation:

```text
e9e1c6f52  2026-08-10  Fix one-shot updates and remote state synchronization
48eda9783  2026-08-10  Add fast RME downloads and robust host transactions
```

Published release tag: `v6.6.3-RME-b5`.

Release-documentation commit: `8e553d9ac`.

## Build 6 release update

Build 6 adds allocation-free RME protocol parsing and a dedicated regression
suite for the service paths that previously exhausted memory during serial
connection, synchronization, motion, and file transfer:

* parameter, signed/unsigned number, transaction, percent-decoding, USB path,
  SHA-256, and service-frame parsing now share fixed-capacity production code;
* unsigned fields reject negative and overflowing values while retaining the
  complete 32-bit transaction-ID range;
* percent-decoded values fail instead of silently truncating, and filesystem
  paths remain confined below `/usb`;
* RME service frames remain isolated from ordinary and line-numbered G-code;
  and
* eight host test cases cover 400,081 assertions, including 100,000 repeated
  synchronization parses to detect retained parser state.

Connect command payloads and transfer state also retain the fixed-buffer,
allocation-free implementations introduced during the memory-hardening work.
The adjacent transfer suite passes 514,726 assertions and the Connect suite
passes 268 assertions.

The final release command was:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

All 15 6.6.3 presets passed. Maximum MINI flash use was 98.66%, MK4 used
60.76%, MK3.5 used 56.13%, XL used 68.86%, and CORE One INDX used 65.43%.
The staged release directory contains exactly 15 BBFs under `bbf/6.6.3`.

Build 6 firmware continuation:

```text
dd83ff5c1  2026-08-10  Remove transfer subsystem heap dependencies
4dc69250b  2026-08-10  Remove Connect command heap churn
a1cd5aeea  2026-08-10  Add RME protocol regression tests
```

Published release tag: `v6.6.3-RME-b6`.

## Build 7 release update

Build 7 fixes a deterministic crash when a Connect inline download is
restarted while its partial file is still owned by the transfer subsystem.
The retry path previously attempted to construct a second `PartialFile`, which
violated the intentionally singleton fixed-DMA-buffer design and stopped with
`Concurrent partial files`.

Retries now reopen the existing partial file in place without allocating or
constructing a second transfer object. Before resuming, the firmware verifies
the file size, contiguity, backing USB logical unit, and first-sector mapping.
This keeps transfer recovery allocation-free and prevents unrelated motion
commands from appearing to trigger the transfer failure when they merely
overlap a Connect retry.

The final release command is:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

All 15 6.6.3 presets passed. Maximum MINI flash use was 98.67%, MK4 used
60.76%, MK3.5 used 56.14%, XL used 68.86%, and CORE One INDX used 65.44%.
The staged release directory contains exactly 15 BBFs under `bbf/6.6.3`.

Build 7 firmware continuation:

```text
7f4b0ed44  2026-08-10  Fix partial-file retry singleton collision
```

Published release tag: `v6.6.3-RME-b7`.

## Build 8 release update

Build 8 separates the RME communications lease from the printer's operating
state. Session replies now report `lease=1 printer_state=IDLE` instead of the
ambiguous `active=1`, which generic host parsers could incorrectly interpret
as machine activity and use to keep an idle printer and its lighting active.

Session commands, event subscriptions, keepalive timing, and out-of-band
framing remain unchanged. Hosts should use `lease` only for connection health
and `printer_state` for the actual machine state. The serial protocol and host
integration documentation have been updated accordingly.

The final release command is:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

All 15 6.6.3 presets passed. Maximum MINI flash use was 98.68%, MK4 used
60.77%, MK3.5 used 56.14%, XL used 68.86%, and CORE One INDX used 65.44%.
Exactly 15 BBFs were staged under `bbf/6.6.3`.

Build 8 firmware continuation:

```text
2d7c1834a  2026-08-10  Disambiguate RME session and printer state
```

Published release tag: `v6.6.3-RME-b8`.

## Build 9 release update

Build 9 makes failed RME uploads diagnosable and resumable. Binary NACKs now
distinguish offset, CRC, size, chunk, and completion-offset errors. Fatal
binary, bulk, and legacy text failures report the committed offset and a
specific decode, storage-write, size, or hashing reason instead of the generic
`write_failed` response.

Fatal raw failures now close the file and restore line mode. The raw binary
abort frame and the exact `@RME FILE ABORT` compatibility escape both suspend
the partial upload, preventing the printer from remaining trapped in binary
mode when a host's raw writer has already failed. A matching BEGIN reopens and
re-hashes the partial, reports its nonzero resume offset, and may resume using
binary, bulk, or legacy text transport. An explicit line-mode ABORT still
discards the partial.

Resume metadata is persisted beside the partial file at BEGIN, allowing an identical
BEGIN to recover after USB disconnect or firmware restart. Binary uploads now
also accept CRC-protected RME control frames at reserved offset `0xfffffffe`,
so out-of-band UI, status, and recovery commands remain responsive without
placing command bytes in the uploaded file. The FILE service can export a
retained Buddy crash dump directly to `/usb` for subsequent binary download.

The RME unit suite covers stable failure classification, reserved control
offsets, and the malformed-raw ASCII abort escape. It passes 400,127 assertions
across 11 cases.

The final release command is:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

Published release tag: `v6.6.3-RME-b9`.

## Build 10 release update

Build 10 fixes corruption of pipelined RME text and bulk upload commands. The
serial reader previously reset the live line-buffer index before dispatching a
completed RME frame. If filesystem work serviced the scheduler recursively,
the next pipelined command could overwrite the frame still being parsed and
its Base64 suffix could escape into Marlin as `echo:Unknown command`.

Serial command draining is now explicitly non-reentrant, and each completed
RME frame is copied to an immutable fixed-capacity stack snapshot before
dispatch. This keeps the advertised four-command bulk window safe without heap
allocation or a throughput-reducing per-chunk round trip. A regression test
proves nested dispatch is rejected and that the reader becomes available on
the next scheduler pass.

The final release command is:

```text
./build.py --final --versions 6.5.7 6.6.3 --jobs 15
```

All 15 6.6.3 presets passed. Maximum MINI flash use was 99.42%, MK4 used
61.10%, MK3.5 used 56.48%, XL used 69.23%, and CORE One INDX used 65.80%.
Exactly 15 BBFs were staged under `bbf/6.6.3`.

Build 10 firmware continuation:

```text
c50d75ee4  2026-08-12  Fix reentrant RME bulk parser corruption
```

Published release tag: `v6.6.3-RME-b10`.

## Build 11 release update

Build 11 keeps RME upload implementation files private. Prusa Connect and RME
directory listings no longer expose `FWUPD.RME`, `FWUPD.UI`, `.rme-part`,
`.rme-meta`, or `.rme-old` artifacts, and Connect file-change events and
explicit file-info requests cannot publish them either. Known partial and
metadata paths remain available to explicit idle RME cleanup, while the
verified firmware candidate remains protected by the idempotent `FIRMWARE
UNSTAGE` workflow.

The host documentation now defines a complete durable recovery workflow:
OctoPrint plugins persist path, size, and SHA-256 before BEGIN, resume with an
identical BEGIN at the returned committed offset, or recover through text/bulk
and issue line-mode ABORT to remove partial data and metadata together. Lost
host provenance can be handled by explicit STAT/DELETE of mechanically derived
sidecar paths without exposing those paths in ordinary listings or deleting a
rollback file speculatively.

All 15 6.6.3 presets passed. Maximum MINI flash use was 99.42%, MK4 used
61.08%, MK3.5 used 56.46%, XL used 69.21%, and CORE One INDX used 65.80%.
Exactly 15 BBFs were staged under `bbf/6.6.3`.

Build 11 firmware continuation:

```text
561565bfb  2026-08-12  Hide private RME transfer artifacts
```

Published release tag: `v6.6.3-RME-b11`.

## Build 12 release update

Build 12 closes the remaining text-bulk upload corruption path. The advertised
four-command, 384-byte window could leave roughly 1.7 KiB of encoded commands
waiting while one command was committed to storage, but the TinyUSB CDC RX
FIFO held only 512 bytes. Under storage latency the FIFO could lose an `@RME`
prefix and join Base64 fragments, exposing payload text as `Unknown command`
and leaving the upload in `upload_state`.

The RX FIFO is now 2048 bytes while TX remains 512 bytes, preserving the full
bulk window and transfer speed without restoring the former 8 KiB allocation.
Bulk size/window constants are shared with a compile-time receive-capacity
assertion. A general ten-second upload watchdog now suspends abandoned
text/bulk transfers, releases the shared transfer latch, and preserves the
committed partial for a matching-BEGIN resume.

Machine discovery now reports the zero-origin slicer-usable build volume on
every model instead of raw homing, docking, wiping, purge, or service travel.
For example, CORE One reports 250 mm in X, MINI reports 180 mm in Z, and XL
reports 360 mm in Z.

The RME suite passes 400,145 assertions across 15 cases. All 15 6.6.3 presets
passed. Maximum MINI flash use was 99.43%, MK4 used 61.09%, MK3.5 used 56.46%,
XL used 69.21%, and CORE One INDX used 65.80%. Exactly 15 BBFs were staged
under `bbf/6.6.3`.

Build 12 firmware continuation:

```text
40ff92fa6  2026-08-13  Fix RME bulk receive window corruption
```

Published release tag: `v6.6.3-RME-b12`.

## Build 13 release update

Build 13 closes a FAT long-filename visibility bypass in Connect directory
reports. The private-artifact filter previously classified only `dirent::d_name`,
which can contain an unrelated 8.3 alias such as `FWUPD~1.RME`, while Connect
published the long name `FWUPD.RME.rme-part` or `FWUPD.RME.rme-meta` as
`display_name`. Connect now classifies both the published long filename and its
short alias, so protected candidates, partial payloads, resume metadata, and
rollback files remain invisible to Connect while explicit RME STAT, resume,
ABORT, DELETE, and UNSTAGE workflows remain available.

The Connect suite passes 277 assertions across 47 cases. All 15 6.6.3 presets
passed. Maximum MINI flash use was 99.43%, MK4 used 61.09%, MK3.5 used 56.46%,
XL used 69.21%, and CORE One INDX used 65.80%. Exactly 15 BBFs were staged
under `bbf/6.6.3`.

Build 13 firmware continuation:

```text
966a63ff6  2026-08-13  Hide RME artifacts by FAT long filename
```

Published release tag: `v6.6.3-RME-b13`.

## Build 14 release update

Build 14 adds fast, guarded INDX loadcell filament monitoring without changing
the existing maximum-flow protection. During meaningful extrusion, the new
runout detector qualifies pressure for 0.75 seconds and 0.75 mm, then requires
more than one second and one millimetre of missing pressure before stopping.
The independently selectable movement-collapse detector retains its longer,
conservative debounce, while maximum-flow breakout remains always enabled.
`M591 S` controls loadcell runout and `M591 U` controls movement monitoring.

`M1601 R1`, `R2`, and `R3` preserve runout, filament-not-moving, and flow-limit
causes through the common safe Continue/Unload/Abort recovery workflow. Each
condition has distinct on-printer guidance and reports a distinct RME
workflow/code pair: `filament_runout/runout`,
`filament_movement/not_moving`, or `extrusion_flow_limit/flow_limit`.
Protocol, G-code, and plugin-integration documentation describe the settings,
timing, and routing contract.

The focused classifier suite passes 19 assertions across 10 cases. The exact
combined release command completed both maintained lines with no failures. All
15 6.6.3 presets passed and exactly 15 BBFs were staged under `bbf/6.6.3`.
Maximum flash use was MINI 99.44%, MK4 61.14%, MK3.5 56.47%, XL 69.26%, and
CORE One INDX 65.86%.

Build 14 firmware continuation:

```text
10f8a83ca  2026-08-14  Add INDX loadcell filament workflows
```

Published release tag: `v6.6.3-RME`.

## Binary transfer hardening update

Binary uploads now use the same production rolling-header scanner validated on
the maintained 6.8.1 line. Corrupt lengths or offsets slide the ten-byte window
one byte at a time, keeping framed abort and control recovery reachable without
trusting an unvalidated payload length. Abort frames require a zero-length
payload and control frames require a non-empty payload.

The negotiated upload window is now three 512-byte frames. Its complete
1,566-byte wire backlog fits the configured 2 KiB TinyUSB CDC RX FIFO and is
enforced by a compile-time assertion, retaining continuous pipelining without
the packet loss that allowed binary payload bytes to escape into line parsing.
Chunk and window values come from the shared transfer contract used by the
receiver, capability report, resume response, tests, and host compatibility
suite.

Upload state has head and tail guards. A detected overwrite restores line
mode, releases the shared transfer latch, and preserves durable resume metadata
instead of dereferencing damaged FILE or SHA state. New stream tests exercise
ordinary frames, abort/control validation, arbitrary corrupt prefixes, byte
loss, duplication, CRC failure, and recovery using the production scanner.

Release gates pass 400,179 RME assertions across 18 cases, 514,733 transfer
assertions across 11 cases, 278 Connect assertions across 47 cases, and all
103 adjacent OctoPrint compatibility tests. The shared parser/transfer headers
retain 100% line and function coverage and 87.3% branch coverage.

The canonical `./build.py --final --versions 6.6.3 --jobs 15` matrix passes
15/15 presets and stages exactly 15 BBFs. Maximum flash use is translated MINI
at 99.68%; MK4 uses 61.24%, MK3.5 uses 56.57%, XL uses 69.36%, and CORE One
INDX uses 65.95%. XL has the highest aggregate RAM use at 84.45%.
### Firmware-update handoff race fix

- Fixed a cross-task race where `M997 /usb/FWUPD.RME` published the cleanup
  marker before reset, allowing the running application to delete the verified
  candidate while the bootloader was about to open it.
- The retained exact-file request is now armed before the cleanup marker is
  atomically published. Application cleanup explicitly preserves the candidate
  while that retained request still selects `FWUPD.RME`.
- Marker-publication failures cancel the retained request and leave the
  candidate available for a safe retry instead of rebooting into a missing
  image. The private temporary marker is hidden from Connect and removable by
  `@RME FIRMWARE UNSTAGE`.

### Marlin task stack crash fix

- Decoded a field crash as `vApplicationStackOverflowHook` for the Marlin task,
  with its saved stack pointer 84 bytes beyond the former allocation after an
  RME session/query sequence.
- Increased the Marlin task stack from 1360 to 1664 words, providing 1216
  additional bytes while remaining inside the existing CCMRAM budget.
- Centralized the allocation requirement and added a regression invariant that
  preserves at least 1024 bytes of crash-derived guard space.

### Numeric editor contrast fix

- Fixed focused numeric spinner values, including custom-preheat temperatures,
  rendering white-on-white while being edited. Edited values now use the
  active menu color scheme's contrasting foreground color.

# RME Upstream Release Playbook

Use this when Prusa publishes a new Firmware Buddy release and the RME firmware needs to be rebuilt quickly on top of it.

## Goal

Bring the RME feature set onto the new upstream release with the smallest practical diff, build all supported firmware images, and produce release notes and BBFs on day one.

## Current Baseline

RME 6.5.3 is based on upstream tag `v6.5.3` at `3fc7b43a3`.

The first current-branch commit by Rylan Meilutis is:

```text
706eb6ea5d25403a8924b82e29f3a23786fe594c new octoprint screen and better color scheme
```

The audit baseline for this playbook is the parent of that commit:

```text
3fc7b43a3b9bd77e9267647af5d96fe1ee7cb1c2
```

To re-audit the full RME patch surface:

```sh
base=$(git rev-parse $(git rev-list --reverse --author='Rylan Meilutis' HEAD | head -1)^)
git diff --stat "$base"
git diff --name-status "$base"
git diff --dirstat=files,0 "$base"
```

At the time this playbook was written, that range covered 485 files with roughly 7k insertions. The largest categories were GUI resources/theme assets, serial printing, LED/light-bar behavior, build tooling, safety/chamber logic, config-store additions, and GUI framework polish.

Primary RME branch:

```sh
coreone-v6.5.3-patches
```

Primary release notes:

```sh
RELEASE_NOTES_v6.5.3-RME.md
```

## Supported Release Targets

Build these presets for a full release:

```sh
coreone
mini
mini-en-cs
mini-en-de
mini-en-es
mini-en-fr
mini-en-it
mini-en-pl
mini-en-ja
mini-en-uk
xl
mk4
mk3.5
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
git log --oneline v6.5.3..coreone-v6.5.3-patches
git cherry-pick <first-rme-commit>^..<last-rme-commit>
```

If the old branch has extra experimental commits, cherry-pick the feature groups below instead of the whole range.

3. Before resolving conflicts, regenerate the changed-file inventory from the old release branch and compare it to this playbook.

```sh
base=$(git rev-parse $(git rev-list --reverse --author='Rylan Meilutis' coreone-v6.5.3-patches | head -1)^)
git diff --name-status "$base"..coreone-v6.5.3-patches > /tmp/rme-files.txt
git diff --dirstat=files,0 "$base"..coreone-v6.5.3-patches
```

Every non-resource source directory in that inventory should map to one of the feature groups below. If a file does not fit, add a new playbook item before finishing the rebase.

4. Resolve conflicts by feature area. Do not resolve mechanically; upstream often changes GUI, FSM, and Marlin integration code between releases.

5. Run the narrow compile checks early.

```sh
./build.py --preset xl --bootloader yes --jobs 13
./build.py --preset coreone --bootloader yes --jobs 13
./build.py --preset mini --bootloader yes --jobs 13
```

XL is the flash headroom gate. MINI is the layout and small-display/screen-only brightness gate. Core One is the chamber/door/LED behavior gate. MK4 or MK3.5 is the non-side-LED status/display brightness gate.

6. Run the full release build after focused targets pass.

```sh
./build.py --preset coreone,mini,mini-en-cs,mini-en-de,mini-en-es,mini-en-fr,mini-en-it,mini-en-pl,mini-en-ja,mini-en-uk,xl,mk4,mk3.5 --bootloader yes --jobs 13
```

7. Update the release notes with the new upstream version, upstream tag/commit, RME feature list, known limitations, and build results.

8. Stage BBFs from `bbf/` and verify every expected preset produced a matching `.bbf`.

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
Blocking startup commands do not hide the serial print screen.
M77 and complete M73 end serial print state.
M601 parks and keeps the serial print screen active.
MMU/runout recovery sends the host resume action.
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
Host paths normalize onto `/usb/`, reject parent-directory traversal, and create upload directories as needed.
```

### LEDs, Chamber Lights, Side Strips, And GPIO Light Bar

Keep per-state lighting, temporary per-print percent overrides, status LED separation, and post-print acknowledgement behavior.

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
Core One/Core One Plus chamber and status temporary print brightness values are percent-based and independent.
XL side-strip temporary print brightness is percent-based.
Temporary print lighting overrides reset after the print.
Per-state screen brightness is available on supported displays for Deep Idle, Idle, Active, and Printing.
Active-state screen brightness is clamped to at least 15%.
If idle/deep-idle screen brightness is below 15%, the first touch or encoder input only wakes/brightens the screen and must not trigger the focused action.
Canceled/aborted prints show abort indication until door open/close acknowledgement or new print.
Finished state acknowledgement still follows platform rules.
Opening and closing the door during filtering turns status LEDs off.
Per-state pages expose Deep Idle, Idle, Active, Printing.
Timing settings live one level above per-state settings where users compare state entry and exit timing.
GPIO light bar state control remains independent of chamber/side LEDs.
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
Core One Plus disables manual open-vent prompts.
Serial prints disable manual open-vent prompts.
Automatic close-vent behavior remains available where upstream supports it.
Core One and Core One Plus selftest/setup labels remain correct.
```

### Chamber Fan, Filtration, And Safety Timer

Keep the chamber fan/filtering behavior and bed-heater safety timer changes.

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
Bed-heater timeout is independently configurable through UI and M86 B.
M86 B0 disables the bed-heater timeout.
Paused/active prints keep bed safety timer reset.
Filtering can continue after print finish.
Filtering status LED indication remains independent from chamber/side LEDs.
Chamber fan/filter settings display and reset correctly.
Waiting-for-chamber and waiting-for-bed status messages still report progress.
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
Screen brightness is independent from chamber/side/status LED brightness.
Active screen brightness cannot be configured or applied below 15%.
Dim idle/deep-idle wake input is consumed before normal UI action dispatch.
Abort indication is not treated as finished indication.
Door/filter acknowledgement is Core One-specific unless explicitly handled for another platform.
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

## Manual Smoke Test Matrix

Minimum hardware or simulator checks before publishing:

```text
Core One / Core One Plus:
  Serial print start, pause, resume, cancel, finish.
  Manual open-vent prompt suppression for serial print and Core One Plus.
  Chamber LEDs off while status LEDs still work.
  Per-print chamber/status brightness at 0%, low value, and 100%.
  Per-state screen brightness: active below 15% is clamped, idle/deep-idle below 15% consumes first input as wake-only.
  Door open/close acknowledgement after finished, aborted, and filtering states.
  External chamber light does not flicker off/on at print start or print finish.
  Bed-heater safety timer UI and M86 B behavior.
  Theme import, lock settings, and text/PIN input.
  External GPIO light bar configuration on supported hardware.

XL:
  Serial print start, pause, resume, cancel, finish.
  MMU/runout recovery sends serial host resume.
  MMU extra purge shows status progress instead of staying at 0%.
  Per-print side-strip brightness at 0%, low value, and 100%.
  Per-state screen, status, and side-strip brightness settings remain visible and independent.
  Idle/deep-idle screen brightness below 15% wakes without activating the touched/focused UI control.
  Chamber fan/filter controls and filtering LED indication.
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
  Idle screen brightness below 15% consumes first encoder input as wake-only.
  Theme resources render correctly on the small display.
  Status LED color settings and other status-LED-only code are not instantiated when `HAS_LEDS` is disabled.

MK4 / MK3.5:
  Serial print screen and post-print Continue acknowledgement.
  Abort indication does not look like finished indication.
  Status LEDs acknowledge finished/aborted states correctly on non-door platforms.
  Screen and status LED brightness controls are visible, but side/chamber controls are hidden unless the target actually supports them.
  LED manager builds and runs the non-side-LED wake path.
  Serial printing and theme changes compile under MK feature flags.
  Shared external-light code compiles under MK3.5 include paths even if the feature is not user-facing there.
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
Core One Plus and vent behavior.
Build/signing notes.
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
./build.py --preset xl --bootloader yes --jobs 13
./build.py --preset coreone --bootloader yes --jobs 13
./build.py --preset mini --bootloader yes --jobs 13
./build.py --preset mk4 --bootloader yes --jobs 13
./build.py --preset coreone,mini,mini-en-cs,mini-en-de,mini-en-es,mini-en-fr,mini-en-it,mini-en-pl,mini-en-ja,mini-en-uk,xl,mk4,mk3.5 --bootloader yes --jobs 13
```

Confirm `bbf/` contains all expected BBFs and that the build summary reports zero failures.

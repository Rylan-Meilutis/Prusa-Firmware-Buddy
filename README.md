# Buddy

## RME Firmware Features

This branch contains the RME custom firmware based on Prusa Firmware Buddy 6.5.7. It keeps the standard Buddy firmware foundation while adding features for OctoPrint workflows, lighting control, screen dimming, chamber behavior, fleet configuration, and printer maintenance.

The main additions over the base firmware are:

- **Improved OctoPrint and serial printing:** Dedicated serial-print screens with `Legacy`, `Messages Only`, and `Progress` modes; a neutral waiting page at the start of each serial job instead of stale status from the preceding job; more reliable print start, pause, resume, cancel, and finish handling; serial `M601`/`M602` park and resume without recursively triggering OctoPrint pause/resume scripts; serial `M77` completion that stays acknowledged without blocking later commands; streamed `M73` progress no longer prematurely ends a print before host end G-code finishes; persistent finished summaries with duration, completion time, and remaining filtration time; host-preferred progress and ETA reporting with streamed `M73` fallback; filtered host messages; and, on loadcell/MMU/toolchanger platforms, `//action:notification` state/progress reporting for heating and heat soak, probing, MMU loads, tool changes, filament runout, and MMU/tool-change errors.
- **OctoPrint SD/USB storage support:** Host-facing `M20` through `M32` commands allow compatible serial hosts to list files, upload G-code to USB media, select files, start print-from-SD workflows, report progress, seek, and delete files. Serial uploads enter write mode immediately on `M28` so fast host streams are written to USB instead of being acknowledged and discarded.
- **Prusa Connect compatibility:** Prusa Connect registration, telemetry, and websocket requests report the stock upstream firmware version for cloud-side compatibility and feature gating, while the printer UI, local APIs, M115, crash dumps, and release artifacts keep the RME firmware identity. Serial prints report normal print stages to Connect instead of generic busy state. Chamber-light telemetry reports the currently driven brightness; an energized external GPIO chamber light reports 100% even when the side strip is off. Remote brightness values wake the light without overwriting per-state settings. Connect uploads still require writable USB media mounted as `/usb`.
- **Per-state lighting and screen brightness:** Configure `Deep Idle`, `Idle`, `Active`, and `Printing` brightness independently. LED values have no cross-state ordering limits, so Printing lights may be dimmer or fully off while Idle lights remain on. Supported printers expose only the controls their hardware can use. Active and Printing screen brightness cannot be set below `15%`, while Idle and Deep Idle can turn the screen fully off.
- **Temporary per-print dimming:** Tune-menu percentage overrides can temporarily dim or disable the screen and supported printer lighting without overwriting persistent defaults. Lighting overrides are isolated from screen brightness, and a screen override below `15%` wakes to a readable `15%` floor on the first interaction while remaining responsive for the rest of the print.
- **Status-strip color themes:** Built-in and imported themes can set the in-progress RGB status-strip / light-bar color on supported printers. Indigo uses deep indigo while the other built-in profiles retain the standard cyan default. Imported JSON themes expose this as `led.printing`.
- **Post-print lighting behavior:** Status LEDs indicate filtering and print completion separately from chamber lighting. The smooth filtering pulse shifts from warning yellow toward white as the filtration timer expires, providing an approximate at-a-glance indication of chamber air cleanup progress. LEDs then return to the normal active/idle/deep-idle brightness sequence after the finished hold. Screen interaction and Core One door activity wake the status strip again. The finished screen can end filtration early without being dismissed, opening the Core One door during filtration offers the same choice, and closing the door dismisses only that early-exit prompt.
- **External GPIO light bar control:** Supported xBuddy printers can configure an external chamber light bar through GPIO breakout / IO expander hardware, including independent per-state enable settings, flicker-resistant output handling, and temporary print override behavior that only pairs the external light bar with chamber/side LEDs for the active print.
- **Chamber and filtration improvements:** Expanded chamber fan controls, configurable filtration behavior, `M154.8` G-code control for starting or stopping a configured filtration cycle, Core One / Core One Plus selection with distinct `COREONE+` reporting, Core One Plus automatic vent handling that defers to explicit `M870` commands, and prompt-free serial-print vent handling.
- **PID management:** View, edit, reset, and autotune supported hotend and heatbed PID values from `Settings -> PID Settings`, next to Input Shaper and Phase Stepping. PID Settings opens separate Hotend and Heatbed submenus so each heater is managed independently. Autotune displays progress and lets the user save or discard the resulting values.
- **Fleet configuration export:** RME settings can be exported to `/usb/rme_settings.gcode` and replayed as G-code when configuring multiple printers.
- **Per-print extrusion calibration and monitoring:** Loadcell-equipped printers expose `M976` for stationary, RAM-only pressure-advance calibration using continuous PrusaPATuner-derived slow/fast excitation and executed E-step alignment. An optional `S` parameter sets and waits for the selected physical hotend. Batch `A` manifests validate every expected material and temperature before doing anything, then run the printer's normal XL tool changes or full MMU unload/load sequence for all used filaments in one command. The resulting healthy pressure response arms print-time detection of missing pressure rise, pressure collapse from grinding/tangles, and high-flow pressure breakout; detected faults enter the normal stuck-filament pause workflow. A conservative material flow ceiling remains active in the planner. The slicer can calibrate every physical tool and logical MMU filament used by file or serial prints, while profile `M572` values remain the fallback. Purge-bin machines test over the bin; other supported machines test beyond the printable boundary and finish with a locally probed front-edge anchor.
- **Persistent filament colors:** Loaded material records include a per-tool/MMU color chosen from a built-in palette or eight user-defined persistent slots. Colors are set during loading or from Loaded Filament management, can be set and queried with `M865`, and custom definitions are included in RME settings export/import. Flash-constrained MINI builds keep Black/White selection in the LCD menu; their custom slots and full RGB selection remain available through `M865` and RME settings import.
  A guided **Control > Pressure Advance Calibration** screen sits immediately above Calibrations & Tests and presents every loaded tool/MMU slot as an on/off selection followed by one Run action. The blocking progress screen takes over as soon as the batch begins, keeps chamber lighting active, remains visible over idle, file-print, and serial-print screens, and permits only Abort until it presents the selected tools' aggregated results. Manual MMU runs probe before loading and unload once before the result; all results can be saved together to `/usb/pa-calibration.gcode` or dismissed. Loaded-filament machines probe at the material profile's lower nozzle-preheat temperature, create 10 mm of bed/nozzle clearance, and only then heat to the test temperature; MMU machines probe unloaded with the same clearance before full heating. Filament-sensor sampling remains live during the excitation, but its runout/autoload events are suppressed until cleanup to avoid false toolhead-sensor errors. Temperatures remain limited to ±15 °C around the assigned material profile, M976 restores every previous hotend target on all exit paths, and results are reported to three decimal places with 0.002-second search resolution and a 0.500 safety ceiling.
  See the [PrusaSlicer and OrcaSlicer setup guide](doc/auto_pa_slicer_setup.md) for single-tool, XL/INDX, and MMU start-G-code templates.
- **Safety, UI, and maintenance refinements:** Configurable hotend and bed-heater safety timeouts under Heater Safety, both capped at 60 minutes, improved paused-print and manual-intervention behavior that keeps the bed hot while coordinating nozzle recovery and host resume, theme updates and theme import, lock settings, screen wake protection, and active lighting during setup, calibration, self-test, and MMU workflows.

Feature availability depends on printer hardware. The branch targets Original Prusa MINI/MINI+, MK3.5, MK3.9, MK4, XL, CORE One, and CORE One L / CORE One Plus configurations. See the detailed notes attached to each RME release for model-specific behavior, G-code commands, flashing notes, and build validation.

## Building RME Firmware with `build.py`

Use the top-level `./build.py` wrapper to build release firmware for installation. It builds up to four intended physical-printer presets at a time by default, packages the firmware, normalizes the output names, and stages the resulting `.bbf` files in `./bbf`. A shared non-blocking lock prevents a second wrapper or version-worktree build from concurrently mutating build and BBF directories; a competing invocation exits with the lock path instead of corrupting either build.

After the build finishes, the wrapper prints each machine's result, build time, flash usage, aggregate RAM usage, individual memory-region usage, total elapsed wall-clock time, and the absolute path of every staged `.bbf` artifact.

Build all RME release images:

```bash
./build.py --final
```

`--final` keeps the `-RME` firmware badge while removing development build-number suffixes from the running firmware version.

Build only selected printers:

```bash
./build.py --preset coreone,xl,mk4 --final
```

Useful options:

- `--list-presets` lists the available release presets and aliases.
- `--jobs N` adjusts the number of parallel printer builds. The default is `--jobs 4` to avoid saturating the build machine.
- `--skip-bootstrap` skips dependency bootstrap when the local build environment is already prepared.
- `--output-dir PATH` changes the BBF staging directory.
- `--setup-signing` creates a machine-local ECDSA signing key at `.local/firmware-signing-key.pem`.
- `--no-signing-key` explicitly builds unsigned BBFs.
- `--dry-run` prints the commands without starting a build.

The wrapper automatically looks for a supported Python 3.8-3.12 interpreter when the current interpreter is too new for the repository's pinned dependencies. Set `BUDDY_PYTHON=/path/to/python3.12` to select one explicitly.

The wrapper also exposes the managed `.venv` tools to CMake, including Nunavut `nnvg`, by prepending `.venv/bin` to child build `PATH` and passing `Python3_ROOT_DIR` unless the caller overrides it with `-D Python3_ROOT_DIR:PATH=...`.

The lower-level `python3 utils/build.py` command remains available for development, compatibility checks, and specialized presets. Use `./build.py` for normal RME release builds and BBF staging.

The running firmware reports the short RME version. Prusa Connect intentionally receives the stock upstream version string so Connect does not disable cloud features because of the custom `-RME` suffix. The stock Prusa bootloader may append the mandatory BBF build number in its pre-install firmware list; changing that display requires a bootloader change.

This repository includes source code and firmware releases for the Original Prusa 3D printers based on the 32-bit ARM microcontrollers.

The currently supported models are:
- Original Prusa MINI/MINI+
- Original Prusa MK3.5
- Original Prusa MK3.9
- Original Prusa MK4
- Original Prusa XL
- Prusa CORE One/CORE One +
- Prusa CORE One L

## Getting Started

### Requirements

- Python 3.8 through 3.12. If your default `python3` is newer, run `./build.py` so it can select a compatible interpreter, or set `BUDDY_PYTHON` to a Python 3.8-3.12 executable.
- system installation of Python's `requests` package (use either pip or your system package manager)

### Cloning this repository

Run `git clone https://github.com/prusa3d/Prusa-Firmware-Buddy.git`.

### Building (on all platforms, without an IDE)

Run `python utils/build.py`. The binaries are then going to be stored under `./build/products`.

- Without any arguments, it will build a release version of the firmware for all supported printers and bootloader settings.
- Use `--build-type` to select build configurations to be built (`debug`, `release`).
- Use `--preset` to select for which printers the firmware should be built.
- By default, it will build the firmware in "prerelease mode" set to `beta`. You can change the prerelease using `--prerelease alpha`, or use `--final` to build a final version of the firmware.
- Use `--host-tools` to include host tools in the build (`png2font`, ...)
- Find more options using the `--help` flag!

#### Examples:

Build the firmware for MINI and XL in `debug` mode:

```bash
python utils/build.py --preset mini,xl --build-type debug
```

Build the firmware for MINI using a custom version of gcc-arm-none-eabi (available in `$PATH`) and use `Make` instead of `Ninja` (not recommended):

```bash
python utils/build.py --preset mini --toolchain cmake/AnyGccArmNoneEabi.cmake --generator 'Unix Makefiles'
```

#### Windows 10 troubleshooting

If you have python installed and in your PATH but still getting cmake error `Python3 not found.` Try running python and python3 from cmd. If one of it opens Microsoft Store instead of either opening python interpreter or complaining `'python3' is not recognized as an internal or external command,
operable program or batch file.` Open `manage app execution aliases` and disable `App Installer` association with `python.exe` and `python3.exe`.

### Development

The build process of this project is driven by CMake and `build.py` is just a high-level wrapper around it. As most modern IDEs support some kind of CMake integration, it should be possible to use almost any editor for development. Below are some documents describing how to setup some popular text editors.

- [Visual Studio Code](doc/editor/vscode.md)
- [Vim](doc/editor/vim.md)
- [Eclipse, STM32CubeIDE](doc/editor/stm32cubeide.md)
- [Other LSP-based IDEs (Atom, Sublime Text, ...)](doc/editor/lsp-based-ides.md)

#### Contributing

If you want to contribute to the codebase, please read the [Contribution Guidelines](doc/contributing.md).

#### XL and Puppies

With the XL, the situation gets a bit more complex. The firmware of XLBuddy contains firmwares for the puppies (Dwarf and Modularbed) to flash them when necessary. We support several ways of dealing with those firmwares when developing:

1. Build Dwarf/Modularbed firmware automatically and flash it on startup by XLBuddy (the default)
    - The Dwarf & ModularBed firmware will be built from this repo.
    - The puppies are going to be flashed on startup by the XLBuddy. The puppies have to be running the [Puppy Bootloader](http://github.com/prusa3d/Prusa-Bootloader-Puppy).

2. Build Dwarf/Modularbed from a given source directory and flash it on startup by XLBuddy.
    - Specify `DWARF_SOURCE_DIR`/`MODULARBED_SOURCE_DIR` CMake cache variable with the local repo you want to use.
    - Example below would build modularbed's firmware from /Projects/Prusa-Firmware-Buddy-ModularBed and include it in the xlBuddy firmware.
    ```
    cmake .. --preset xl_release_boot -DMODULARBED_SOURCE_DIR=/Projects/Prusa-Firmware-Buddy-ModularBed
    ```
    - You can also specify the build directory you want to use:
    ```
    cmake .. --preset xl_release_boot \
        -DMODULARBED_SOURCE_DIR=/Projects/Prusa-Firmware-Buddy-ModularBed  \
        -DMODULARBED_BINARY_DIR=/Projects/Prusa-Firmware-Buddy-ModularBed/build
    ```
3. Use pre-built Dwarf/Modularbed firmware and flash it on startup by xlBuddy
    - Specify the location of the .bin file with `DWARF_BINARY_PATH`/`MODULARBED_BINARY_PATH`.
    - For example
    ```
    cmake .. --preset xl_release_boot -DDWARF_BINARY_PATH=/Downloads/dwarf-4.4.0-boot.bin
    ```

4. Do not include any puppy firmware, and do not flash the puppies by XLBuddy.
    ```
    -DENABLE_PUPPY_BOOTLOAD=NO
    ```
    - With the `ENABLE_PUPPY_BOOTLOAD` set to false, the project will disable Puppy flashing & interaction with Puppy bootloaders.
    - It is up to you to flash the correct firmware to the puppies (noboot variant).

5. Keep bootloaders but do not write firmware on boot.
    ```
    -DPUPPY_SKIP_FLASH_FW=YES
    ```
    - With the `PUPPY_SKIP_FLASH_FW` set to true, the project will disable Puppy flashing on boot.
    - You can keep other puppies that are not debugged in the same state as before.
    - Use puppy build config with bootloaders (e.g. `xl-dwarf_debug_boot`) on one or more puppies.
    - Recommend breakpoint at the end of `puppy_task_body()` to prevent buddy from resetting the puppy immediately when puppy stops on breakpoint.

See /ProjectOptions.cmake for more information about those cache variables.

#### Running tests
See the detailed testing guide in our [comprehensive testing guide].

[comprehensive testing guide]: tests/unit/README.md

## Flashing Custom Firmware

To install custom firmware, you have to break the appendix on the board. Learn how to in the following article https://help.prusa3d.com/article/zoiw36imrs-flashing-custom-firmware.

## Feedback

- [Feature Requests from Community](https://github.com/prusa3d/Prusa-Firmware-Buddy/labels/feature%20request)

## Credits

- [Marlin](https://marlinfw.org/) - 3D printing core driver
- [Klipper](https://www.klipper3d.org/) - input shaper code based on Klipper

## License

The firmware source code is licensed under the GNU General Public License v3.0 and the graphics and design are licensed under Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0). Fonts are licensed under different license (see [LICENSE](LICENSE.md)).

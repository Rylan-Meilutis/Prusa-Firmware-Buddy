# Buddy

## RME Firmware Features

This branch contains the RME custom firmware based on Prusa Firmware Buddy 6.5.3. It keeps the standard Buddy firmware foundation while adding features for OctoPrint workflows, lighting control, screen dimming, chamber behavior, fleet configuration, and printer maintenance.

The main additions over the base firmware are:

- **Improved OctoPrint and serial printing:** Dedicated serial-print screens with `Legacy`, `Messages Only`, and `Progress` modes; more reliable print start, pause, resume, cancel, and finish handling; persistent finished summaries with duration, completion time, and remaining filtration time; host-preferred progress and ETA reporting with streamed `M73` fallback; filtered host messages; and visible progress during MMU purge and other print-adjacent operations.
- **OctoPrint SD/USB storage support:** Host-facing `M20` through `M32` commands allow compatible serial hosts to list files, upload G-code to USB media, select files, start print-from-SD workflows, report progress, seek, and delete files.
- **Per-state lighting and screen brightness:** Configure `Deep Idle`, `Idle`, `Active`, and `Printing` brightness independently. LED values have no cross-state ordering limits, so Printing lights may be dimmer or fully off while Idle lights remain on. Supported printers expose only the controls their hardware can use. Active and Printing screen brightness cannot be set below `15%`, while Idle and Deep Idle can turn the screen fully off.
- **Temporary per-print dimming:** Tune-menu percentage overrides can temporarily dim or disable the screen and supported printer lighting without overwriting persistent defaults.
- **Post-print lighting behavior:** Status LEDs indicate filtering and print completion separately from chamber lighting, then return to the normal active/idle/deep-idle brightness sequence after the finished hold. Screen interaction and Core One door activity wake the status strip again. The finished screen can end filtration early without being dismissed, and opening the Core One door during filtration offers the same choice.
- **External GPIO light bar control:** Supported xBuddy printers can configure an external chamber light bar through GPIO breakout / IO expander hardware, including independent per-state enable settings and flicker-resistant output handling.
- **Chamber and filtration improvements:** Expanded chamber fan controls, configurable filtration behavior, Core One / Core One Plus selection with distinct `COREONE+` reporting, automatic vent refinements, and suppression of manual vent prompts for serial prints and Core One Plus.
- **PID management:** View, edit, reset, and autotune supported hotend and heatbed PID values from the printer UI. Autotune displays progress and lets the user save or discard the resulting values.
- **Fleet configuration export:** RME settings can be exported to `/usb/rme_settings.gcode` and replayed as G-code when configuring multiple printers.
- **Safety, UI, and maintenance refinements:** Configurable bed-heater timeout, improved paused-print safety behavior, theme updates and theme import, lock settings, screen wake protection, and active lighting during setup, calibration, self-test, and MMU workflows.

Feature availability depends on printer hardware. The branch targets Original Prusa MINI/MINI+, MK3.5, MK3.9, MK4, XL, CORE One, and CORE One L / CORE One Plus configurations. See the detailed notes attached to each RME release for model-specific behavior, G-code commands, flashing notes, and build validation.

## Building RME Firmware with `build.py`

Use the top-level `./build.py` wrapper to build release firmware for installation. It builds up to four intended physical-printer presets at a time by default, packages the firmware, normalizes the output names, and stages the resulting `.bbf` files in `./bbf`.

After the build finishes, the wrapper prints each machine's result, build time, flash usage, aggregate RAM usage, individual memory-region usage, and the absolute path of every staged `.bbf` artifact.

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

The lower-level `python3 utils/build.py` command remains available for development, compatibility checks, and specialized presets. Use `./build.py` for normal RME release builds and BBF staging.

The running firmware reports the short RME version. The stock Prusa bootloader may append the mandatory BBF build number in its pre-install firmware list; changing that display requires a bootloader change.

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
- Use `--host-tools` to include host tools in the build
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

2. Use pre-built Dwarf/Modularbed firmware and flash it on startup by xlBuddy
    - Specify the location of the .bin file with `DWARF_BINARY_PATH`/`MODULARBED_BINARY_PATH`.
    - For example
    ```
    cmake .. --preset xl_release_boot -DDWARF_BINARY_PATH=/Downloads/dwarf-4.4.0-boot.bin
    ```

3. Do not include any puppy firmware, and do not flash the puppies by XLBuddy.
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

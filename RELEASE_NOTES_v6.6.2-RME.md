# 6.6.2-RME Firmware for Prusa CORE One, XL, MK4, MK3.9, MK3.5 and MINI

## Summary

  * New features and improvements
    * Added **Filament -> Loaded Filament(s)** for viewing and reassigning the printer's stored loaded material without unloading or reloading filament
    * Added `M865 Q` so serial hosts can query the current loaded-filament material for each enabled tool or filament slot
  * Fixes
    * Suppressed filament-runout `M600` injection while a print is paused, parking, or resuming so toolhead work during a pause does not immediately trigger a new runout event

This RME branch was created from the current `rme-v6.6.1` branch because no local or remote `v6.6.2` upstream ref was available in this environment. Rebase this branch onto upstream `v6.6.2` before publishing if Prusa releases a distinct 6.6.2 base.

## Loaded Filament Reassignment

The Filament menu now exposes **Loaded Filament(s)** directly. On single-tool printers it shows the current loaded material. On multi-tool and MMU configurations it opens a per-tool or per-slot list.

Selecting a loaded filament opens a material picker. Choosing a material changes the firmware's stored loaded-filament assignment only; it does not heat, purge, load, unload, or move filament.

_NOTE: The firmware currently stores loaded material per tool, but it does not persist a separate loaded color per tool. Color shown during print setup still comes from G-code metadata or load prompts._

## Serial Loadout Query

Serial hosts can request the printer-side loaded-filament material with:

```gcode
M865 Q
```

The firmware replies with one line per enabled tool or filament slot:

```text
loaded_filament T0 S"PLA"
loaded_filament T1 S"PETG"
```

Hosts can use this for future OctoPrint integration and pre-print validation.

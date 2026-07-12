# 6.6.2-RME Firmware for Prusa CORE One, XL, MK4, MK3.9, MK3.5 and MINI

## Summary

- **New features**
  - Loaded filament overview and material reassignment in the **Filament** menu
  - Serial G-code query for the currently loaded filament materials
- **Fixes**
  - Filament runout is no longer triggered while a print is paused, parking, or resuming

This is the stable RME release of firmware **6.6.2-RME** for the **Prusa CORE One, XL, MK4, MK3.9, MK3.5 and MINI**, bringing targeted filament-management improvements for printers used with serial hosts and multi-tool workflows.

## Loaded Filament Overview And Reassignment

The **Filament** menu now includes **Loaded Filament(s)**. On single-tool printers, the item shows the currently stored loaded material. On multi-tool and MMU configurations, it opens a list of tools or filament slots.

Selecting a loaded filament opens a material picker. Choosing a material updates the firmware's stored loaded-filament assignment only. It does not heat the nozzle, purge, load, unload, or move filament, so it can be used when the user needs to correct the material assignment without physically reloading the tool.

_NOTE: The firmware stores the loaded material per tool or slot. It does not currently store a separate loaded color per tool; color shown during print setup still comes from G-code metadata or load prompts._

## Serial Loaded-Filament Query

Serial hosts can query the printer-side loaded-filament assignments with:

```gcode
M865 Q
```

The firmware replies with one line per enabled tool or filament slot:

```text
loaded_filament T0 S"PLA"
loaded_filament T1 S"PETG"
```

This gives OctoPrint and other serial hosts a stable way to read the printer's current filament loadout for future pre-print validation and host-side workflow support.

## Filament Runout Suppressed During Pause

Filament runout detection no longer injects a new filament-change event while a print is paused, parking for pause, or resuming. This prevents a printer from immediately reporting runout when the user intentionally handles filament or the toolhead during a paused print.

Runout detection remains active during normal printing.

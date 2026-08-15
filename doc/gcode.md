# GCode Grammar

## Basic GCode
Basic GCode parser does not handle options, only parses the command and decides what is body.

```
GCode: Ws* LineNum? Ws* Command Ws* Body Ws* Checksum? Ws* Comment?

LineNum: r"N-?[0-9]+"
Comment: r";.*"
Checksum: r"\*[0-9]+"

Command: CommandLetter CommandCodenum CommandSubcode?
CommandLetter: r"[A-Z]"
CommandCodenum: r"[0-9]+"
CommandSubcode: r"\.[0-9]+"

Body: r"[^;*]*?" # Non-greedy - exclude trailing whitespace

Ws: r"[ \f\n\r\t\v]"
```

## GCode with options
When parsing gcode options, the `Body` from the basic parser must match `BodyWithOptions`:
```
BodyWithOptions: (OptionKey Ws* OptionValue? Ws*)*¨

OptionKey: r"[A-Z]"
OptionValue: BasicOptionValue | QuotedOptionValue

BasicOptionValue: r"[0-9+\-.,]"
QuotedOptionValue: r"\"([^\"]|\\.)*\""
```

## RME filament loadout query

`M865` manages filament type metadata and the firmware's currently loaded
filament assignments. RME extends it with a host-readable loadout query:

```gcode
M865 Q
```

The response contains one line per enabled tool, including the persistent color name and RGB value:

```text
loaded_filament T0 S"PLA" O"Orange" H"#ff8000"
loaded_filament T1 S"PETG" O"Custom" H"#123456"
```

`T` is the zero-based tool or filament slot index. `S` is the firmware's
currently assigned loaded material name. Hosts such as OctoPrint can use this
to discover the printer-side filament loadout before starting or validating a
job.

The loaded material can also be reassigned over serial without loading or
unloading filament:

```gcode
M865 S"PETG" L0
```

This sets the currently loaded material for tool `0` to `PETG`.

Add `O` to assign its color in the same command. `O` accepts a built-in color name, `#RRGGBB`, or the legacy decimal RGB representation:

```gcode
M865 S"PETG" L0 O"#ff8000"
```

Eight persistent custom palette slots are available:

```gcode
M865 V0 N"Galaxy Blue" O"#193a8a"
M865 S"PLA" L0 O"#193a8a"
```

Custom definitions are included in **Settings -> Export RME Settings**. They can also be created and selected under **Filament -> Loaded Filament(s) -> Color**.

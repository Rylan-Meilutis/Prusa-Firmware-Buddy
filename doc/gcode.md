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

The response contains one line per enabled tool:

```text
loaded_filament T0 S"PLA"
loaded_filament T1 S"PETG"
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

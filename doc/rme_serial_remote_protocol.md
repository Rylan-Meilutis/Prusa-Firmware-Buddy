# RME Out-of-Band Serial Control Protocol

RME 6.5.7 and 6.6.3 expose a serial-local control channel for host plugins. It
is intentionally separate from G-code: every frame starts with `@RME`, is
consumed in the serial receiver, and is never placed in the motion-command
queue. This lets a host query or control the UI while `M109`, `M190`, probing,
MMU work, or another foreground G-code is blocking.

Use the printer host's normal serialized transport. A plugin must not open a
second writer on the same port. When OctoPrint adds `N...*checksum` framing,
RME validates and advances that sequence before dispatching the `@RME` payload.
Every accepted frame receives `ok`. Plugins detect support with the read-only
`@RME MACHINE QUERY` handshake.

## UI control

Remote input is disabled after every boot and whenever the UI locks. Enable it
for the current connection before sending input:

```text
@RME UI ENABLE 1
@RME UI ENCODER 1
@RME UI ENCODER -1
@RME UI CLICK
@RME UI BACK
@RME UI HOME
```

Encoder values are clamped to -100 through 100 and zero is rejected. Commands
are transferred through a bounded single-producer/single-consumer mailbox and
applied only by the GUI task. Firmware-owned print, calibration, filament, and error
FSMs continue updating while locked; locking suppresses user input, not screen
state transitions.

## UI lock

```text
@RME LOCK QUERY
@RME LOCK NOW
@RME LOCK UNLOCK pin=1234 digits=4
@RME LOCK SET pin=1234 digits=4 timeout=300 serial=1 enabled=1
```

The PIN is never returned. `digits` accepts 4 through 9. `timeout` is seconds;
`serial` controls serial unlocking and `enabled` controls the persistent lock
feature. While locked, queries and a valid PIN unlock remain available, while
remote UI input and configuration mutation are rejected.

## Theme and lighting

```text
@RME THEME QUERY
@RME THEME SET primary=#3366CC progress=#00AA55 warning=#FFAA00 error=#DD2222 image=#101018
@RME LIGHT QUERY
@RME LIGHT TEMP screen=0 chamber=0 status=0
@RME LIGHT SET screen=0x0014643C chamber=0x00146464 status=0x00146464
```

Colors accept `#RRGGBB` or an integer value. Persistent `LIGHT SET` values pack
four percentage bytes in `0xDDIIAAPP` order: deep idle, idle, active, and
printing. Each byte is 0 through 100. `LIGHT TEMP` applies volatile per-print
overrides and does not change saved settings. Sending an ordinary chamber-light
brightness command retains RME's wake-only behavior; plugin configuration uses
the explicit frames above.

## Filament preset synchronization

```text
@RME FILAMENT QUERY
@RME FILAMENT SET slot=0 name=PLAplus nozzle=215 preheat=170 bed=60 visible=1
```

`QUERY` reports each built-in and persistent host/user slot as `user`, `slot`,
and `name`; temperatures and visibility are supplied when the host synchronizes
a user slot.
`slot` is 0 through 7. Names are at most seven characters and contain no spaces.
Temperatures are degrees Celsius. Synchronized visible presets appear in the
same material selectors used when loading filament and assigning loaded tools.

## Machine discovery

```text
@RME MACHINE QUERY
```

The response is split into `RME_MACHINE`, `RME_ENVELOPE`, and `RME_LIMITS`
records. It reports physical hotend and logical-tool counts, whether the machine
uses a single nozzle, reachable XYZ bounds, and the currently configured maximum XYZ feedrates used by OctoPrint's
printer profile. Firmware-only acceleration tuning remains local.
An OctoPrint plugin can use these read-only values to populate its printer
profile without hard-coded model tables. Values are snapshots: query again
after changing firmware motion limits.

## Prompts and recovery workflows

The plugin can render and answer the printer's current prompt without guessing
which screen is visible:

```text
@RME DIALOG QUERY
@RME DIALOG RESPOND A"Retry"
@RME DIALOG RESPOND S0
```

`QUERY` reports the actions offered by the active firmware prompt. A response
is accepted only when it belongs to that prompt. MMU failures use this same
guarded dialog workflow:

```text
@RME DIALOG QUERY
@RME DIALOG RESPOND A"Retry"
@RME DIALOG RESPOND A"Unload"
@RME DIALOG RESPOND A"MMU_disable"
```

Stuck-filament recovery and tool mapping provide dedicated operations:

```text
@RME STUCK QUERY
@RME STUCK CONTINUE
@RME STUCK UNLOAD
@RME STUCK ABORT
@RME TOOLMAP QUERY
@RME TOOLMAP SET logical=1 physical=3
@RME TOOLMAP ENABLE value=1
@RME TOOLMAP RESET
```

Unsupported features and unknown frames return a nonfatal
`echo:RME_ERROR code=...` record followed by `ok`. They never use Marlin's
`Error:` prefix, so OctoPrint can report the plugin error without cancelling an
active print.

## Host integration rules

- Treat `@RME` as a control protocol, never as slicer start/end G-code.
- Serialize it through the existing host connection so line numbers and
  checksums stay coherent.
- Enable remote UI only while an authenticated plugin session needs it.
- Prefer named recovery actions (`M876 A"..."`) for firmware dialogs; general
  navigation is useful for screens without a dedicated service action.
- Poll queries at a modest rate. The receiver remains available during blocking
  G-code, but it is not a telemetry streaming channel.
- Use normal emergency and recovery commands for safety actions. The remote UI
  channel does not bypass firmware safety checks or screen ownership.

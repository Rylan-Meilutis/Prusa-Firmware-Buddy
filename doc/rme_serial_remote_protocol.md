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

## Session and event subscription

Open a protocol session before relying on structured asynchronous events:

```text
@RME SESSION OPEN events=15 legacy=0
@RME SESSION QUERY
@RME SESSION KEEPALIVE
@RME SESSION CLOSE
```

`events` is a bit mask: progress `1`, error `2`, workflow lifecycle `4`, and
informational notification `8`. `15` subscribes to all event classes.
`legacy=0` replaces only legacy `//action:notification` output while the
session is active. Send `KEEPALIVE` at least every 10 seconds. The 30-second
session lease closes automatically after a host disconnect, restores legacy
notifications, and disables remote UI input. `QUERY` reports the lease state
but does not renew it. Standard `ok`, `busy`, resend requests, temperature/status
reports, and `//action:pause`, `paused`, `resume`, `resumed`, and `cancel`
remain on their established Marlin/OctoPrint paths. Use `legacy=1` while
developing a handler that wants both representations. A reboot always closes
the volatile session.

Events have a monotonically increasing per-session sequence number:

```text
RME_EVENT seq=1 type=workflow workflow=tool_change state=open message="Tool change in progress"
RME_EVENT seq=2 type=progress workflow=mmu state=active progress=50 message="MMU loading filament"
RME_EVENT seq=3 type=error workflow=mmu state=waiting code=mmu_error message="FINDA FILAM. STUCK"
RME_EVENT seq=4 type=progress workflow=mmu state=feeding_to_nozzle code=feeding_to_nozzle progress=75 message="MMU loading filament"
RME_EVENT seq=5 type=workflow workflow=chamber_vent state=open progress=100 message="Chamber vent open"
RME_EVENT seq=6 type=progress workflow=filtration state=active code=post_print progress=60 message="Post-print filtration"
```

The sequence lets a handler detect missed records. Query the active dialog
after reconnecting or after a sequence gap; events are intentionally not
replayed from firmware RAM.

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

## Lifetime and job statistics

```text
@RME STATS QUERY
```

The read-only response is split into `RME_STATS`, `RME_STATS_OPERATIONS`, and
`RME_STATS_FAILURES` records. Distances and extruded filament are reported in
meters; lifetime, current-job, and filter-use times are seconds. Operations
include successful MMU changes and tool picks. Machines with waste-bin tracking
also report the current pellet count.

Failure fields are intentionally separate rather than combined into a
misleading total. Crash, power-panic, and `*_total` MMU counters are persistent.
MMU `*_since_reset` counters follow the printer's existing statistics-reset
semantics. Unsupported optional hardware fields are omitted, except
`filtering_time_s`, which is zero
on machines without filtration. `jobs_started` is the persistent job sequence
counter and is not a successful-print count.

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

Workflow identifiers are stable handler-routing keys. Current firmware emits
`mmu`, `filament_load`, `filament_unload`, `tool_change`, `filament_runout`,
`stuck_filament`, `pressure_advance`, `probing`, `heating`, `firmware_update`,
`waste_bin`, `chamber_vent`, `filtration`, and the generic `printer` fallback.
MMU progress events expose stable snake-case states for idler, selector,
FINDA, extruder, nozzle, cut, eject, homing, ramming, and hardware-test phases;
repeated reports are limited to state changes or five-percent progress changes.
Filtration uses `code=mid_print` or `code=post_print` and reports commanded fan
PWM as a percentage. The event announces the workflow; the active
firmware FSM remains authoritative for its actions. After an error or a
`state=waiting` event, issue `@RME DIALOG QUERY`, render the returned actions,
and answer with `@RME DIALOG RESPOND A"<action>"`. This provides specialized
host presentation without duplicating firmware recovery policy.

Unsupported features and unknown frames return a nonfatal
`echo:RME_ERROR code=...` record followed by `ok`. They never use Marlin's
`Error:` prefix, so OctoPrint can report the plugin error without cancelling an
active print.

## Host integration rules

- Treat `@RME` as a control protocol, never as slicer start/end G-code.
- Serialize it through the existing host connection so line numbers and
  checksums stay coherent.
- Enable remote UI only while an authenticated plugin session needs it.
- Open one event session per serial connection and close it before disconnect.
- On a structured error/waiting event, query the dialog instead of assuming
  that every error with the same workflow offers identical actions.
- Prefer named recovery actions (`M876 A"..."`) for firmware dialogs; general
  navigation is useful for screens without a dedicated service action.
- Poll queries at a modest rate. The receiver remains available during blocking
  G-code, but it is not a telemetry streaming channel.
- Use normal emergency and recovery commands for safety actions. The remote UI
  channel does not bypass firmware safety checks or screen ownership.

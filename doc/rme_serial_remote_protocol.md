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
@RME SESSION OPEN events=31 legacy=0
@RME SESSION QUERY
@RME SESSION KEEPALIVE
@RME SESSION CLOSE
```

`events` is a bit mask: progress `1`, error `2`, workflow lifecycle `4`,
informational notification `8`, and configuration changes `16`. `31`
subscribes to all event classes; the former `15` value remains valid for older
handlers that do not consume the change stream.
`legacy=0` replaces only legacy `//action:notification` output while the
session is active. Send `KEEPALIVE` at least every 10 seconds. The 30-second
session lease closes automatically after a host disconnect, restores legacy
notifications, and disables remote UI input. `QUERY` reports the lease state
but does not renew it. Standard `ok`, `busy`, resend requests, temperature/status
reports, and `//action:pause`, `paused`, `resume`, `resumed`, and `cancel`
remain on their established Marlin/OctoPrint paths. Use `legacy=1` while
developing a handler that wants both representations. A reboot always closes
the volatile session.

The session lease is deliberately independent of printer activity. `OPEN`,
`KEEPALIVE`, `QUERY`, and other read-only RME frames do not wake the display or
lights, refresh the serial-print idle timer, or keep the printer active. Only
an actual remote UI input or requested machine action counts as activity.

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
replayed from firmware RAM. Generic notification/progress events report the
real device state (`idle`, `printing`, `paused`, `busy`, `attention`, and so
on). Session replies use `RME_SESSION lease=1 printer_state=IDLE`: `lease`
reports only that the communications channel is open, while `printer_state`
reports the actual machine state. The former ambiguous `active` key is no
longer emitted.

### INDX loadcell filament and flow errors

After a valid INDX pressure calibration, optional loadcell filament policies
and the always-on calibrated flow limit publish distinct error workflows before
opening the common Continue/Unload/Abort recovery FSM:

```text
RME_EVENT seq=20 type=error workflow=filament_runout state=waiting code=runout message="Loadcell detected filament runout"
RME_EVENT seq=21 type=error workflow=filament_movement state=waiting code=not_moving message="Loadcell detected filament not moving"
RME_EVENT seq=22 type=error workflow=extrusion_flow_limit state=waiting code=flow_limit message="Extrusion flow-pressure limit detected"
```

These workflow/code pairs are stable and mutually distinct. Plugins should
route on them rather than parsing `message` or translated GUI text. The later
load/unload progress belongs to the shared recovery implementation; it does not
change or clear the original cause. Retain the cause until recovery closes or
the print aborts. `M591 S` controls the runout detector, `M591 U` controls the
movement-collapse detector, and neither setting disables flow-limit detection.

## Configuration change stream

After opening a session with event bit `16`, firmware announces accepted local
and host-side mutations without requiring periodic polling:

```text
RME_CHANGE seq=7 revision=12 domain=manufacturer key=custom origin=local
RME_CHANGE seq=8 revision=13 domain=theme key=colors origin=host tx=481
```

`revision` increases for every announced mutation during the boot. `seq`
shares the normal per-session event sequence, so either a sequence gap or a
nonconsecutive configuration revision means the host must refresh its snapshots.
`domain` selects the existing authoritative query (`MANUFACTURER`, `FILAMENT`,
`THEME`, `LIGHT`, or `LOCK`), and `key` narrows the changed portion. `origin`
is `local` for front-panel/firmware changes and `host` for an accepted RME
mutation. A host may append `tx=<nonzero u32>` to a mutating RME command; the
same value is echoed on its change record so the initiating client can match
the acknowledgement without suppressing updates for other clients.
The accepted range is `1..4294967295`. RME parses it as an unsigned value, so
IDs above `2147483647` remain intact. Outside the file service's explicitly
advertised windows, wait for `ok` before issuing another persistent mutation;
an earlier `RME_CHANGE` is state notification, not transmit credit.

Take a complete snapshot at startup, reconnect, session expiry, or sequence/
revision loss. During a healthy session, apply `RME_CHANGE` and refresh only
its named domain; do not continuously poll settings. A change event is emitted
after persistent storage accepts the mutation. Unknown/rejected commands do
not advance the revision.

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

`LIGHT QUERY` is a versioned, complete snapshot. Its first `RME_LIGHT` record
retains the legacy `screen_persistent`, `chamber_print`, `screen_print`, and
`status_print` fields, and adds `schema=2`, channel support flags, and the saved
packed `screen`, `chamber`, and `status` matrices. Packed values are emitted as
unsigned decimal integers but have the same `0xDDIIAAPP` layout accepted by
`LIGHT SET`. Four `RME_LIGHT_STATE` records provide the decoded percentages:

```text
RME_LIGHT_STATE state=deep_idle screen=20 chamber=20 status=20
RME_LIGHT_STATE state=idle screen=20 chamber=20 status=20
RME_LIGHT_STATE state=active screen=100 chamber=100 status=100
RME_LIGHT_STATE state=printing screen=60 chamber=100 status=100
```

The snapshot also emits:

```text
RME_LIGHT_POLICY activity_timeout_s=120 event_timeout_s=300 off_timeout_s=120 door_holds_active=1 post_print_hold=1 status_finished_hold_s=300
RME_LIGHT_LIVE state=idle screen=20 chamber=20 print_screen=60 print_chamber=100 print_status=100
```

Unsupported channels are identified by the `*_supported=0` fields and use
zero for their saved matrix and `-1` for unavailable live/override values.
Plugins should query once after opening a session, cache this snapshot, and
query it again only after an `RME_CHANGE domain=light` notification (or after
reconnecting). A host-originated change includes its `tx` value, so the host
can acknowledge its own mutation without polling or creating a synchronization
loop.

## Filament preset synchronization

```text
@RME FILAMENT QUERY
@RME FILAMENT CREATE slot=0 name=PLAplus base=PLA nozzle=215 preheat=170 bed=60 heatbreak=45 chamber_min=15 chamber_max=38 chamber_target=20 filtration=0 abrasive=0 flexible=0 visible=1
```

`CREATE` and `SET` both update a persistent firmware user profile; `CREATE` is
provided for clearer host workflows. `QUERY` reports each built-in and user
slot with its complete supported parameter set. On firmware with upstream base
preset support, `base` selects the preset material family whose hidden machine
parameters are inherited. `brand` is accepted as a compatibility alias for
`base` because some hosts use that label, but it must contain a built-in family
such as `PLA`, `PETG`, or `ASA`, not arbitrary vendor metadata.
`slot` is 0 through 7. Names are at most seven characters and contain no spaces.
Temperatures are degrees Celsius; use `-1` for an unset optional chamber bound.
Synchronized visible presets appear in the same local material selectors used
when loading filament and assigning loaded tools, and remain usable without a
serial host.

## Filament manufacturers

Manufacturer is independent spool metadata stored alongside loaded material
and color. Firmware provides 50 common manufacturers, ordered with
Prusa/Prusament, eSUN, Polymaker, SUNLU, Bambu Lab, Overture, and Atomic first,
plus eight persistent user-defined slots. RME matching is case-insensitive;
percent-encode spaces and other non-token characters.

The local load and MMU preload manufacturer pickers list built-ins and saved
custom entries and include **Add Manufacturer**. A newly entered unique name is
stored in the first free custom slot and selected for that load immediately.

```text
@RME MANUFACTURER QUERY
@RME MANUFACTURER CREATE slot=0 name=Local%20Filament
@RME MANUFACTURER ASSIGN tool=0 name=polymaker
@RME MANUFACTURER ASSIGN tool=0 name=none
@RME MANUFACTURER DELETE slot=0
```

`QUERY` emits `RME_MANUFACTURER builtin=<0|1> slot=<n> name=<encoded>` and
`RME_MANUFACTURER_LOADED tool=<n> name=<encoded|none>` records. The legacy
`M865 Q` report includes `M"manufacturer"`. On 6.6.3, `M865 Y<n> N"name"`
creates a custom entry for settings replay and `M865 K<tool> N"name"` assigns
one; these letters avoid colliding with upstream's existing visibility and
base-preset parameters. Existing M865 and RME operations are unchanged.

## USB CDC speed and compatibility

The printer connection is USB CDC rather than a baud-clocked UART. Firmware
advertises 1,000,000 as its preferred normal-mode rate; 115200, 250000, and
other legacy host selections remain compatible fallbacks and do not cap USB
packet throughput. Any line-coding rate other than 57600 selects normal Marlin
communications; 57600 remains reserved for diagnostic USB logging. RME,
numbered G-code, M20-M32, `ok`/resend, and older hosts remain supported. Larger
multi-packet CDC RX/TX FIFOs absorb file and firmware transfer bursts.
`RME_SESSION` replies advertise `preferred_baud=1000000` and a comma-separated
fallback list. Older handlers may ignore these appended fields.

## Machine discovery

```text
@RME MACHINE QUERY
```

The response is split into `RME_MACHINE`, `RME_ENVELOPE`, and `RME_LIMITS`
records. It reports physical hotend and logical-tool counts, whether the machine
uses a single nozzle, the zero-origin slicer-usable XYZ build volume, and the
currently configured maximum XYZ feedrates used by OctoPrint's printer profile.
`RME_ENVELOPE` deliberately excludes homing overtravel, dock, purge, cleaner,
and other service-only coordinates; for example, CORE One reports 250 mm in X
even though its toolhead can travel farther. Firmware-only acceleration tuning
remains local.
For INDX, `hotends` and `tool_capacity` are at most eight and `logical_tools`
is the enabled count selected by the four- or eight-dock calibration workflow.
Marlin's internal `NoTool` sentinel is never reported as a ninth tool.
An OctoPrint plugin can use these read-only values to populate its printer
profile without hard-coded model tables. Values are snapshots: query again
after changing firmware motion limits.

## Lifetime and job statistics

```text
@RME STATS QUERY
```

The read-only response is split into `RME_STATS`, `RME_STATS_OPERATIONS`,
`RME_STATS_FAILURES`, and `RME_STATS_MEMORY` records. Distances and extruded filament are reported in
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
`RME_STATS_MEMORY` reports the instantaneous `heap_free` and fixed
`heap_total` byte counts. It is diagnostic telemetry: hosts may sample it at
connection and around a failed operation, but must not poll it continuously or
infer print state from it.

## USB filesystem and firmware operations

The filesystem service is confined to the user-visible `/usb` volume. Paths
are relative to that root and percent-encode spaces and reserved characters.
Absolute paths, `..`, duplicate separators, internal flash, configuration, and
system partitions are rejected. Discover support with:

```text
@RME FILE CAPS
@RME FILE LIST path=/
@RME FILE STAT path=jobs/part.bgcode
@RME FILE READ path=jobs/part.bgcode offset=0 length=48
@RME FILE READ_BINARY path=jobs/part.bgcode offset=0 length=1024
```

`LIST` emits `RME_FILE_ENTRY` records followed by `RME_FILE_LIST_END`. `READ`
returns at most 48 bytes per request as Base64 in `RME_FILE_DATA`; continue from
`offset + length` until `eof=1`.

RME upload internals are deliberately absent from both RME `LIST` and Prusa
Connect directory reports. This includes the protected `FWUPD.RME` candidate,
its `FWUPD.UI` handoff marker, and every `.rme-part`, `.rme-meta`, or `.rme-old`
sidecar. Hiding them affects discovery only: an idle RME host may explicitly
`STAT`, `READ`, or `DELETE` a known partial, metadata, or rollback path for
diagnosis and cleanup. Use `FIRMWARE UNSTAGE`, not generic `DELETE`, to remove
the verified protected candidate and its associated private files atomically.

For fast downloads, `CAPS` advertises
`binary_read=1 binary_read_chunk=1024`. After `READ_BINARY`, consume the
`RME_FILE_BINARY_READ_READY` line, then switch the receive parser for exactly
one raw frame. Its ten-byte little-endian header contains `offset:u32`,
`length:u16`, and `crc32:u32`, followed immediately by `length` payload bytes.
Verify the offset and CRC, switch back to line parsing, and consume
`RME_FILE_BINARY_READ_COMPLETE next=<offset> eof=<0|1>` plus the normal `ok`.
Request the returned `next` offset until `eof=1`. Each request is deliberately
bounded to 1024 bytes, so control traffic is never blocked by an entire large
download and a failed chunk can be retried independently. Legacy Base64 reads
remain supported unchanged.

The header shows the indigo R and transfer percentage while RME owns an upload
or multi-chunk download. It uses the transfer-status position rather than
adding another competing service indicator; the ordinary session R returns
when the transfer finishes.

Binary-safe, restartable uploads require the final byte count and SHA-256:

```text
@RME FILE WRITE_BEGIN path=jobs/part.bgcode size=12345 sha256=<64 hex digits>
@RME FILE WRITE_CHUNK path=jobs/part.bgcode offset=0 data=<Base64, up to 48 decoded bytes>
@RME FILE WRITE_END path=jobs/part.bgcode
@RME FILE ABORT
```

Negotiated bulk upload removes the per-48-byte round trip while retaining the
commands above as a fallback:

```text
@RME FILE WRITE_BULK_BEGIN path=jobs/part.bgcode size=12345 sha256=<64 hex digits>
@RME FILE WRITE_BULK_CHUNK offset=0 data=<Base64, up to 384 decoded bytes>
@RME FILE WRITE_BULK_CHUNK offset=384 data=<Base64, up to 384 decoded bytes>
@RME FILE WRITE_BULK_END
```

`CAPS` advertises `bulk=1 bulk_chunk=384 bulk_window=4 overwrite=1`. A host
may pipeline four sequential chunks and then waits for the cumulative
`RME_FILE_BULK_ACK` offset before sending the next window. Firmware drains the
bounded CDC FIFO continuously. Its RX backlog is sized for the three complete
encoded commands that may follow the command currently being committed, while
the cumulative ACK prevents a fifth command from entering the window. Storage
latency therefore cannot truncate an in-flight command or disconnect the endpoint.
Completed RME lines are snapshotted before dispatch, and the stateful serial
line reader cannot be entered recursively while filesystem code is committing
a chunk. Consequently, a nested scheduler pass cannot overwrite an in-flight
command or expose its Base64 payload as ordinary G-code. Hosts may use the
advertised window without serializing every chunk on an individual `ok`.
If a plugin ever sees Base64 payload reported as an unknown G-code, it must
treat that as a firmware/transport integrity failure, stop the window, and use
the documented ABORT or matching-BEGIN resume workflow; payload text is never
a valid command. The general `upload_timeout_ms` watchdog also suspends an
abandoned text/bulk upload, releases the shared latch, and retains its durable
committed prefix for a matching BEGIN to resume.
Offset, declared-size, atomic temporary-file, SHA-256, abort, and final-rename
guarantees are identical to legacy upload. Binary upload remains preferred for
maximum throughput.
Signed BBF files use the same bulk upload followed by `@RME FILE FLASH`.

For maximum throughput, `CAPS` also advertises
`binary=1 binary_chunk=1024 binary_window=8`. Start with:

```text
@RME FILE WRITE_BINARY_BEGIN path=firmware/update.bbf size=<bytes> sha256=<64 hex digits>
```

After `RME_FILE_BINARY_READY`, send raw little-endian frames containing
`offset:u32`, `length:u16`, `crc32:u32`, then `length` payload bytes. Payloads
are at most 1024 bytes and eight sequential frames may be in flight. Firmware
emits cumulative `RME_FILE_BINARY_ACK` offsets. A zero-length frame at the
final offset flushes, verifies SHA-256, atomically renames, and returns to line
mode. A zero-length frame with offset `0xffffffff` suspends the partial file.
Recoverable frame failures return `RME_FILE_BINARY_NACK` with the last
committed offset and `reason=offset_mismatch`, `size_exceeded`, `crc_mismatch`,
`chunk_too_large`, or `completion_offset_mismatch`. Legacy text protocols
remain unchanged.

The ten-byte header and payload are raw bytes: do not add CR/LF, Marlin line
numbers/checksums, or Base64. Raw mode owns the receiver until a completion or
abort frame returns it to line mode, so unframed ASCII G-code and service
commands cannot be interleaved during that interval. The printer's independent motion,
heater, and safety tasks continue to execute. Hosts cancel with the binary
`0xffffffff` abort frame and must wait for the abort/completion response before
resuming line traffic. On NACK, discard unacknowledged frames and restart at
the returned committed offset. For compatibility with a host whose raw writer
has failed, the exact line `@RME FILE ABORT` is also recognized after it forms
a malformed binary header. Both abort forms return
`RME_FILE_BINARY_ABORTED offset=<n> resumable=1` and restore line mode; the
ordinary line-mode `ABORT` command still discards the partial upload.

The receiver never trusts a malformed header's declared length. It slides a
bounded ten-byte header window until it finds a plausible committed-offset,
control, or abort frame. A corrupt window produces one diagnostic NACK while
recovery is active; stale frames from the old host window do not produce a
NACK storm. This guarantees that a corrupt length (including 65535) cannot
consume a later abort/control frame indefinitely. If raw input is inactive for
the advertised `binary_timeout_ms`, firmware closes the stream, releases the
shared transfer slot, retains the durable partial and metadata, restores line
mode, and reports:

```text
RME_FILE_BINARY_SUSPENDED offset=<committed> resumable=1 reason=inactivity_timeout
```

A storage-write or SHA-state failure closes the stream, restores line mode,
retains the committed partial, and reports `echo:RME_ERROR workflow=file
code=<reason> offset=<n> resumable=1`. Text transports distinguish
`decode_failed` and `chunk_too_large`; all modes distinguish `size_exceeded`,
`disk_write_failed`, and `hash_failed`. Send a matching BEGIN with the same
path, size, and SHA-256 to resume. Firmware reopens and re-hashes the partial,
then replies with `*_READY offset=<n> resumed=1`. The resumed transport may
differ from the failed one, so binary can fall back to bulk or legacy text
without retransmitting the verified prefix.
If reopening or re-hashing a matching durable partial fails, firmware reports
`echo:RME_ERROR workflow=file code=resume_failed offset=<committed>
resumable=1`, restores line mode, releases the shared transfer latch, and
preserves the partial and metadata. A host must retry the identical BEGIN or
explicitly ABORT; it must not infer an offset-zero restart from this error.
Selecting a different BEGIN only changes the checkpoint currently held in RAM;
it does not delete sidecars belonging to another durable host manifest.
Metadata is written and synced through a private `.rme-tmp` file before atomic
publication. Startup/resume accepts a complete temporary record if power was
lost during publication; hosts and Connect must treat it as private metadata.

Binary mode also reserves `offset=0xfffffffe` for an out-of-band control frame.
Its nonzero payload is one complete ASCII `@RME` command without CR/LF, and its
length and CRC use the normal header fields. Firmware dispatches the command,
does not write or hash those bytes, and terminates the response with
`RME_FILE_BINARY_CONTROL_COMPLETE` and `ok`. Transfer-mutating FILE commands
are rejected in this channel; use `offset=0xffffffff,length=0` or the framed
`@RME FILE ABORT` control for suspension. Ordinary upload offsets and the
legacy binary format are unchanged.

`CAPS` advertises `binary_control=1 binary_control_offset=4294967294
resumable_abort=1 durable_resume=1 shared_transfer_latch=1`. A compact
`.rme-meta` sidecar stores the
declared size, SHA-256, and final path beside `.rme-part`; the committed offset
is recovered from the partial file and its prefix is re-hashed. USB disconnect
automatically suspends raw mode, so reconnect or firmware restart can issue the
same BEGIN and resume safely. Completion or explicit line-mode ABORT removes
the private sidecar.

### Plugin recovery and orphan cleanup workflow

Because private artifacts are intentionally omitted from directory listings,
Connect filters the FAT long filename it publishes, not only the unrelated
8.3 alias returned in `dirent::d_name`. A host must keep a small durable upload
manifest of its own. Record the final
relative path, declared size, SHA-256, and selected transport before sending
BEGIN, and remove that record only after `RME_FILE_WRITE_COMPLETE` or
`RME_FILE_BINARY_COMPLETE`. On startup or reconnect, process each unfinished
manifest entry as follows:

1. Query `FILE CAPS` and wait through `transfer_busy` or `printer_busy` without
   sending payload bytes.
2. Send the same `WRITE_BEGIN`, `WRITE_BULK_BEGIN`, or `WRITE_BINARY_BEGIN`
   using the manifest's exact final path, size, and SHA-256. Firmware validates
   the matching `<path>.rme-meta`, re-hashes `<path>.rme-part`, and returns a
   READY record with `offset=<committed>` and `resumed=1`.
3. To resume, continue from exactly that offset. The transport may be changed
   after reconnect; its BEGIN still uses the same path, size, and SHA-256.
4. To discard instead, deliberately recover with `WRITE_BEGIN` or
   `WRITE_BULK_BEGIN` (not binary BEGIN), then send line-mode `@RME FILE ABORT`
   immediately after READY. This is the preferred cleanup because firmware
   removes the matching partial and metadata together while holding the shared
   transfer latch. A raw binary abort only suspends and remains resumable.

Do not scan Connect or `FILE LIST` for orphan names, and do not parse
`.rme-meta` as a public file format. If the plugin has lost its manifest and
the user supplies the original destination, it may probe the mechanically
derived `<path>.rme-part` and `<path>.rme-meta` names with `FILE STAT`, then
delete those exact paths with `FILE DELETE` while idle. Treat `not_found` as
already clean. Delete neither an active transfer nor an `.rme-old` rollback
file speculatively; the latter is firmware-owned recovery state. For firmware
uploads mapped to `FWUPD.RME`, use `FIRMWARE QUERY` and `FIRMWARE UNSTAGE`
instead of reconstructing or deleting private paths individually.

RME, Connect, Link, and slicer uploads share one storage-transfer latch. Every
RME BEGIN acquires it before opening or recovering a partial file and retains
it through verification and atomic publication. If another transfer owns the
latch, the command returns `echo:RME_ERROR workflow=file code=transfer_busy`
without entering raw mode or changing upload state. Hosts should wait for the
current transfer to finish; they must not retry chunks or arm binary parsing.
The same response defers READ/READ_BINARY, DELETE, RENAME, MKDIR, PRINT,
FLASH, and crash-dump export while storage is owned. `printer_busy` separately
means a write or mutation was attempted while the printer was not idle.

Connect downloads pause network traffic when printing begins while retaining
their slot, partial file, progress, and retry budget. They resume after the
print. Consequently a paused Connect transfer intentionally continues to
return `transfer_busy` to RME and Link until it completes or is stopped.

## Authoritative firmware staging

Firmware stage state is separate from directory listing and ordinary BBF
files. Query the application-owned state with:

```text
@RME FIRMWARE QUERY
RME_FIRMWARE candidate=0 armed=0 state=idle
ok
```

A verified upload to the legacy wire name `FWUPD.BBF` is stored under the
protected, non-bootloader-discoverable `/usb/FWUPD.RME` name. Only that
protected candidate is reported:

```text
RME_FIRMWARE candidate=1 armed=0 state=ready path=FWUPD.RME size=<bytes> sha256=<64-lowercase-hex>
```

`candidate=1` is never inferred from another `.BBF` file. Immediately before
the explicit `FLASH`/`M997` handoff resets the machine, both the cleanup marker
and exact retained bootloader filename have been set and firmware emits:

```text
RME_FIRMWARE candidate=1 armed=1 state=restarting path=FWUPD.RME
RME_FIRMWARE_RESTART reconnect=1
```

`armed=1` requires both the retained one-shot selection for `FWUPD.RME` and its
cleanup marker; directory contents alone cannot arm it. The bootloader clears
the retained request before the next application startup. Candidate cleanup is
retried on startup until the protected file is gone.

An idle host can discard a candidate idempotently:

```text
@RME FIRMWARE UNSTAGE
RME_FIRMWARE_UNSTAGED candidate=0 armed=0
ok
```

UNSTAGE rejects an armed request, a print, or an active RME/Connect/Link
transfer. It briefly owns the shared transfer latch and removes only
`FWUPD.RME`, its private RME partial/metadata/rollback siblings, and its private
cleanup marker. It never scans for or deletes ordinary BBFs. Generic DELETE or
RENAME cannot mutate the protected candidate; use UNSTAGE instead. QUERY may
return `workflow=firmware code=transfer_busy` while storage is owned so hashing
the candidate cannot race an upload.

Chunks must be contiguous. Firmware writes a `.rme-part` sibling, verifies the
size and SHA-256, flushes it to media, and atomically renames it only after a
successful `WRITE_END`. If the destination is an existing regular file,
firmware keeps it as a rollback sibling until the verified replacement is in
place; all legacy, bulk, and binary upload modes therefore support overwrite.
Directories are never overwritten. `ABORT` removes the partial file. Mutating operations
are refused while printing:

```text
@RME FILE MKDIR path=jobs
@RME FILE RENAME path=jobs/old.bgcode dest=jobs/new.bgcode
@RME FILE DELETE path=jobs/new.bgcode
@RME FILE PRINT path=jobs/part.bgcode
@RME FILE FLASH path=firmware/6.6.3-RME.bbf
@RME FILE CRASH_DUMP path=dump_buddy.bin
```

When `crash_dump=1` is advertised, `CRASH_DUMP` exports the retained Buddy
crash dump to the requested `/usb` path and reports
`RME_FILE_CRASH_DUMP_SAVED`; `no_crash_dump` means there is no valid retained
dump, `printer_busy` defers export until filesystem activity is safe, and
`crash_dump_write_failed` identifies media/export failure.

`PRINT` and `FLASH` are validated, copied into the normal firmware command
queue, and acknowledged as queued; they therefore use the same print-state and
bootloader-validation paths as local UI operations. `FLASH` accepts only BBF
files. The filesystem protocol never exposes raw internal storage.

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
`filament_movement`, `extrusion_flow_limit`, `stuck_filament`,
`pressure_advance`, `probing`, `heating`, `firmware_update`,
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

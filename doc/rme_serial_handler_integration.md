# Implementing an RME serial workflow handler

This guide describes the host-side behavior required for complete support of
RME's out-of-band control and recovery protocol. The protocol shares the
printer's existing serial connection but is consumed before the G-code FIFO,
so it remains usable during heater waits, probing, filament operations, and
other blocking commands.

## Connection lifecycle

1. Use the same serialized writer as normal G-code. Never open a competing
   process or serial descriptor.
2. Send `@RME MACHINE QUERY`. If no `RME_MACHINE` response is received, retain
   the host's standard Marlin behavior.
   After discovery, send `@RME STATS QUERY` to populate lifetime and current-job
   statistics without model-specific tables.
3. Send `@RME SESSION OPEN events=31 legacy=0`. Wait for `RME_SESSION active=1`.
4. Track the `seq` field of every `RME_EVENT`. On a gap, query both
   `@RME SESSION QUERY` and `@RME DIALOG QUERY` and refresh the plugin UI.
5. Send `@RME SESSION KEEPALIVE` every 10 seconds. The printer expires the
   lease after 30 seconds without one, restores legacy notifications, and
   disables remote UI input. `QUERY` deliberately does not renew the lease.
   Close the session when deliberately disconnecting.

`RME_SESSION active=1` means only that the protocol lease is live. It is not a
printer-state update. Keepalives and read-only queries are passive and may
continue while the printer transitions to idle and applies its idle display
and lighting policy. Use the `state` on structured events for device state;
do not synthesize an active printer state from protocol traffic.

All `@RME` service frames are excluded from the local chamber-light activity
timer. Explicit remote UI input still wakes the display. If a temporary print
screen override is below 15%, that wake is bounded to 30 seconds and the
requested dim/off value is then restored.

The session is volatile and is reset by firmware reboot. Re-negotiate after
every reconnect. A handler must never infer a session merely because an older
connection opened one.

## Snapshot and change synchronization

Query each configuration domain once after connecting, then use `RME_CHANGE`
as the synchronization clock. Do not poll settings in steady state. Store both
the normal event `seq` and configuration `revision`; refresh all snapshots if
either stream has a gap, and refresh only the named domain after an ordinary
change. Rebuild snapshots after reconnect, lease expiry, or printer reboot.

Attach a unique nonzero `tx=<u32>` to every host mutation. An accepted change
is announced with `origin=host` and the same `tx`; update the initiating UI
from that authoritative event and suppress only the matching pending-request
indicator. Do not discard the event globally, because another plugin view may
depend on it. Front-panel changes carry `origin=local`. Commands without `tx`
remain compatible and still generate a revisioned event.

Use the complete unsigned range `1..4294967295`; do not encode transaction IDs
as signed 32-bit values. Except for explicitly windowed file-transfer frames,
wait for the command's `ok` before sending the next persistent configuration
mutation. Configuration events may precede that `ok`, but they do not grant
another transmit credit. This bounds journal work and input buffering during a
large filament-profile replay.

A front-panel loaded-filament edit emits `RME_CHANGE domain=filament
key=loaded` only after material, color, and manufacturer have all been
committed. Refresh the loaded-filament snapshot on that event; no polling or
delay is required.

## Filesystem integration

Use `@RME FILE CAPS` during discovery and treat `/usb` as the only exported
filesystem. For downloads, request no more than 48 bytes and decode each
`RME_FILE_DATA data=` value from Base64 only as the compatibility fallback.
When `binary_read=1`, prefer `READ_BINARY` with at most `binary_read_chunk`
bytes. Wait for `RME_FILE_BINARY_READ_READY`, consume exactly the advertised
ten-byte little-endian header and payload length, verify its offset and CRC32,
then return to line parsing for `RME_FILE_BINARY_READ_COMPLETE` and `ok`.
Continue at `next` until `eof=1`; retry only the failed offset. Do not send
ordinary commands between READY and the end of that single raw frame. Unlike a
raw upload, a download returns to line mode after every bounded frame, keeping
pause, abort, and workflow controls responsive between requests.

For uploads, calculate the complete byte count and SHA-256 before selecting
the fastest mutually supported mode:

1. Prefer `binary=1`. Send `WRITE_BINARY_BEGIN`, wait for
   `RME_FILE_BINARY_READY`, and temporarily switch the connection from line
   parsing to raw framing.
2. Otherwise use `bulk=1`: pipeline up to `bulk_window` contiguous
   `WRITE_BULK_CHUNK` frames of at most `bulk_chunk` decoded bytes, then wait
   for the cumulative `RME_FILE_BULK_ACK` offset. Never exceed the negotiated
   window, and discard the unacknowledged window if its ACK times out.
3. Fall back to `WRITE_BEGIN` plus acknowledged 48-byte `WRITE_CHUNK` frames.

The raw frame header is exactly ten little-endian bytes:

```text
offset:u32 | length:u16 | crc32:u32 | payload:length
```

Use the `binary_chunk`, `binary_window`, `header`, `endian`, and `crc` values
returned by `CAPS`/`RME_FILE_BINARY_READY`; do not hard-code them as permanent
protocol constants. In this release they are 1024 bytes, eight frames, ten
bytes, little endian, and CRC32. Pipeline only contiguous frames. Treat every
ACK as the cumulative committed offset, and after a NACK discard outstanding
frames and restart from its reported offset. Complete the upload with a
zero-length frame whose offset equals the declared size. Wait for
`RME_FILE_BINARY_COMPLETE` before returning the shared connection to ordinary
line mode. A zero-length frame with offset `0xffffffff` aborts the raw upload.

Raw mode deliberately gives every received byte to the transfer decoder, so
ASCII G-code and `@RME` commands cannot be multiplexed into the byte stream.
Firmware motion, heater, and safety tasks continue running, but a host needing
to cancel the transfer must send the binary abort frame, not an ASCII command.
Keep the normal emergency-disconnect policy as a final transport fallback.
Once line mode resumes, the out-of-band recovery service is available again.

For all three modes, retry from the last acknowledged offset after a recoverable
transport interruption and issue the mode's abort operation when abandoning a
transfer. Never interleave upload modes or send a second transfer request while
one is active.

When `CAPS` advertises `overwrite=1`, uploading to an existing regular-file
path replaces it. Firmware retains the previous destination under a private
temporary sibling until the verified new file is installed, rolls it back if
installation fails, and removes the backup on success. Directories cannot be
overwritten.

Do not present a file as complete until `RME_FILE_WRITE_COMPLETE` arrives.
For bulk and binary modes, the corresponding completion records are
`RME_FILE_BULK_COMPLETE` and `RME_FILE_BINARY_COMPLETE`.
`PRINT` and `FLASH` return a queued acknowledgement before the foreground
firmware workflow starts; continue consuming standard and structured state
events. Percent-encode spaces and reserved path characters. Never attempt to
address paths outside `/usb`.

Upload a signed BBF through the same selected transfer mode, then send
`@RME FILE FLASH path=<encoded path>`. This delegates validation, retained
bootloader request, and restart to the firmware's normal update path. Do not
assume that upload completion itself flashes or reboots the printer.

Before an M997 reset, an active RME session receives
`RME_EVENT ... workflow=firmware_update state=restarting ...` followed by
`RME_FIRMWARE_RESTART reconnect=1` and the command acknowledgement. Stop normal
send-loop timeout handling when the reconnect marker arrives, close the old
serial descriptor after it disappears, and wait for the same USB serial number
to enumerate again before reopening it. Re-run machine discovery, session
negotiation, and the startup snapshot after reconnect. Do not exhaust ordinary
baud autodetection while the bootloader is still installing the image.

`FWUPD.BBF` remains the legacy host-visible staging name, but RME firmware maps
it to the non-discoverable `/usb/FWUPD.RME` file. Merely uploading or staging
that file cannot trigger installation. An explicit FLASH/M997 request creates
the cleanup marker and retained bootloader selection in the same reboot
handoff. The application removes the neutral stage after that attempt. A host
must not submit a second flash request during reconnect.

## Message routing

Continue passing these records through the host's existing Marlin parser:

- `ok`, `busy`, `Resend`, line-number errors, and emergency responses;
- temperature, heater-power, position, SD/USB, and ordinary status reports;
- `//action:pause`, `paused`, `resume`, `resumed`, and `cancel`.

Route `RME_EVENT` and other `RME_*` records to the plugin. With `legacy=0`,
firmware suppresses only the corresponding `//action:notification` text, so
OctoPrint's print-state control remains intact.

Treat `RME_STATS`, `RME_STATS_OPERATIONS`, `RME_STATS_FAILURES`, and
`RME_STATS_MEMORY` as
unordered key/value snapshots. Preserve the suffixes and units in the field
names: `_m` is meters, `_s` is seconds, and `_total` is persistent. A host must
tolerate optional fields being absent on machines
without the corresponding MMU, toolchanger, filtration, or waste-bin hardware.
`heap_free` and `heap_total` are byte counts for diagnostics; collect them on
startup/reconnect or explicit troubleshooting, not as a periodic heartbeat.

## Workflow handlers

Use the `workflow` field to choose presentation, not to invent recovery
actions:

| Workflow | Suggested host UI | Action source |
| --- | --- | --- |
| `mmu` | MMU progress, slot/path help, error details | `DIALOG QUERY` |
| `filament_load` / `filament_unload` | Native load/unload lifecycle, cancellation, skip and failure state | `DIALOG QUERY` only when waiting/error |
| `tool_change` | Tool/dock status and retry/abort help | `DIALOG QUERY`, `TOOLMAP` |
| `filament_runout` | Material replacement workflow | `DIALOG QUERY` |
| `stuck_filament` | Continue, unload, or abort | `STUCK QUERY` / `DIALOG QUERY` |
| `pressure_advance` | Calibration phase and percentage | progress events; abort via active dialog |
| `probing` / `heating` | Noninteractive progress | no assumed action; query if waiting |
| `firmware_update` | Upload/verification/restart phase | firmware-update protocol |
| `waste_bin` | Purge-bin state and empty-bin guidance | active dialog and waste-bin G-code |
| `chamber_vent` | Opening, closing, final position, or actuation failure | no action unless an error is emitted |
| `filtration` | Mid-print/post-print filtering and commanded PWM percentage | no assumed action |
| `printer` | Generic notification fallback | query if `state=waiting` |

An error event is a notification that firmware entered or is about to enter a
recovery state. Always query the current dialog before enabling buttons. Send
named responses (`A"Retry"`) rather than numeric indexes because names remain
stable across UI layouts and translations. Firmware rejects stale or invalid
responses without cancelling the print.

## Reliability requirements

- Preserve command ordering and OctoPrint line-number/checksum handling.
- Do not place `@RME` frames inside sliced files.
- Treat unknown/nonfatal `echo:RME_ERROR` records as plugin errors, not printer
  emergencies.
- Never resume a print solely from a progress event. Wait for the firmware's
  accepted response and normal resumed state transition.
- Keep the plugin usable when structured events are unavailable by falling
  back to standard action notifications.
- Do not suppress standard temperatures or safety traffic in the host parser.
- Do not wrap raw binary frames in `N...*checksum`, append CR/LF, Base64-encode
  them, or wait for a Marlin `ok`; only the binary ACK/NACK records pace them.
- Bound retransmission attempts and surface media/hash failures to the user.
  Never silently report a partial `.rme-part` file as the requested filename.

## Minimum conformance test

Test reconnect and sequence-gap recovery, heater waits, native and MMU
load/unload, every MMU progress code and an MMU-not-responding error, filament
runout, stuck-filament Continue/Unload/Abort, a failed and successful tool
change where supported, PA calibration progress/abort, chamber-vent open/close
and failure, mid/post-print filtration start/PWM/stop, UI lock transitions,
unknown commands, and emergency stop. Repeat with `legacy=1` to
confirm both representations coexist, then with `legacy=0` to confirm only
legacy notifications are replaced.
Also query statistics both idle and during a blocking heater wait, validate
units and optional-field handling, and confirm the query never changes a
counter or print state.
Validate the initial-snapshot/change-stream path with local and host changes,
matching and unknown transaction IDs, revision gaps, session expiry, and
reconnect. Confirm an idle host sends no periodic configuration queries.
Exercise all advertised file modes: legacy 48-byte transfer, four-frame bulk
windows, eight-frame raw windows, CRC NACK/restart, offset NACK/restart, binary
abort, disconnect cleanup, wrong SHA-256, full media, atomic completion, print
queueing, and signed-BBF flash handoff. Confirm that legacy hosts which ignore
new `CAPS` fields continue to work unchanged.

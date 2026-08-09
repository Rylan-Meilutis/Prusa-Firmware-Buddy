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
3. Send `@RME SESSION OPEN events=15 legacy=0`. Wait for `RME_SESSION active=1`.
4. Track the `seq` field of every `RME_EVENT`. On a gap, query both
   `@RME SESSION QUERY` and `@RME DIALOG QUERY` and refresh the plugin UI.
5. Send `@RME SESSION KEEPALIVE` with the host's normal connection health
   cadence. Close the session when deliberately disconnecting.

The session is volatile and is reset by firmware reboot. Re-negotiate after
every reconnect. A handler must never infer a session merely because an older
connection opened one.

## Message routing

Continue passing these records through the host's existing Marlin parser:

- `ok`, `busy`, `Resend`, line-number errors, and emergency responses;
- temperature, heater-power, position, SD/USB, and ordinary status reports;
- `//action:pause`, `paused`, `resume`, `resumed`, and `cancel`.

Route `RME_EVENT` and other `RME_*` records to the plugin. With `legacy=0`,
firmware suppresses only the corresponding `//action:notification` text, so
OctoPrint's print-state control remains intact.

## Workflow handlers

Use the `workflow` field to choose presentation, not to invent recovery
actions:

| Workflow | Suggested host UI | Action source |
| --- | --- | --- |
| `mmu` | MMU progress, slot/path help, error details | `DIALOG QUERY` |
| `tool_change` | Tool/dock status and retry/abort help | `DIALOG QUERY`, `TOOLMAP` |
| `filament_runout` | Material replacement workflow | `DIALOG QUERY` |
| `stuck_filament` | Continue, unload, or abort | `STUCK QUERY` / `DIALOG QUERY` |
| `pressure_advance` | Calibration phase and percentage | progress events; abort via active dialog |
| `probing` / `heating` | Noninteractive progress | no assumed action; query if waiting |
| `firmware_update` | Upload/verification/restart phase | firmware-update protocol |
| `waste_bin` | Purge-bin state and empty-bin guidance | active dialog and waste-bin G-code |
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

## Minimum conformance test

Test reconnect and sequence-gap recovery, heater waits, MMU load/unload and an
MMU error, filament runout, stuck-filament Continue/Unload/Abort, a failed and
successful tool change where supported, PA calibration progress/abort, UI lock
transitions, unknown commands, and emergency stop. Repeat with `legacy=1` to
confirm both representations coexist, then with `legacy=0` to confirm only
legacy notifications are replaced.

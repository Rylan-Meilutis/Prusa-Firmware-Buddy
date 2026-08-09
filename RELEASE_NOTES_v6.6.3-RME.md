# 6.6.3-RME Firmware for Prusa CORE One, CORE One INDX, XL, MK4, MK3.9, MK3.5 and MINI

## Summary

6.6.3-RME rebases the complete RME feature stack onto Prusa Firmware Buddy
6.6.3. It retains the pressure-advance calibration, extrusion monitoring,
serial-print recovery, filament material/color management, lighting, UI themes,
USB/serial firmware update, INDX, and release-build features documented for
6.6.2-RME.

### RME additions in this release

- Added an out-of-band `@RME` serial control protocol for future OctoPrint
  plugin integration. It is parsed before the G-code FIFO, so UI and service
  control remains responsive during blocking heater waits, probing, MMU work,
  and calibration.
- Added bidirectional remote UI navigation with explicit volatile enable,
  bounded GUI-task event delivery, application counters, and automatic disable
  whenever the printer UI locks.
- Added serial query/configuration for UI lock state, themes, persistent screen,
  chamber-light and status-light brightness, and temporary per-print brightness.
- Added host synchronization and reporting of eight persistent filament preset
  slots used by normal loaded-filament selection workflows.
- Added read-only machine discovery for model and tool topology, single-nozzle,
  MMU/toolchanger/INDX capabilities, build envelope, and live motion limits so
  a host plugin can configure its OctoPrint printer profile accurately.
- Added `M115` capability discovery for out-of-band control, remote UI,
  configuration, and filament synchronization.
- Kept the protocol distinct from ordinary G-code so sliced files cannot invoke
  remote UI operations accidentally.

See [RME Out-of-Band Serial Control Protocol](doc/rme_serial_remote_protocol.md)
for the complete command and integration reference.

## Upstream base

- Prusa Firmware Buddy `v6.6.3` (`ff6658da4`)
- RME port merge: `a59ee6f04`

## Validation

- CORE One 6.6.3-RME feature build: passed.
- Full 6.5.7/6.6.3 release matrix: recorded after the final multiversion build.

# RME file-derived adaptive mesh

October 4, 2026 firmware for 6.9.0-RME and 6.10.1-RME advertises `mesh_area=1`
in `@RME MACHINE QUERY` on non-MINI builds. RME Compatibility b118 or later
can analyze local text G-code at print preflight and send:

```gcode
@RME MESH SET x=50 y=40 width=80 height=60
G29 P1
```

Units are millimetres in the same XY frame used by M555. All four values are
required, finite, and inside the reported bed envelope; width/height must be
positive. An unlocked, active RME serial print session is required. A valid
command replies `RME_MESH accepted=1`; invalid requests report
`echo:RME_ERROR workflow=mesh code=invalid_area` or `code=no_serial_job`.

The service command stages the rectangle. The next G29 P1 consumes it in the
foreground, updating the native print area before probing. It cannot alter a
probe already running. End/cancel cleanup clears unconsumed bounds. Explicit
G29 XY/size arguments still take precedence. M555 remains supported.

The plugin injects the service command immediately before an ordinary file
G29 P1, not during M110/startup negotiation. Analysis includes positive-extrusion
XY segments across all tools, skirts/brims and wipe towers. Pure travel and
stationary purge are excluded. I/J arcs use a conservative full-circle envelope.
A 1 mm line-width allowance is added; firmware retains its native grid spacing,
reachable-point limits and one-major-grid-point margin. This selects arbitrary
rectangles on the existing mesh grid; it does not create a new variable-pitch grid.

Explicitly sized/extension probes are left alone. Older firmware and MINI,
remote/binary files, unresolvable coordinates, inch/workspace transforms,
unsupported arcs/macros, off-bed deposition, or analysis exceeding 5 seconds
or 100 MiB keep the original slicer/firmware meshing behavior. The plugin logs
whether analysis succeeded. No original print file or extrusion command is edited.

Validation: host bounds/dispatch tests and native area validation tests cover
the interface; physical probing, compatibility with third-party G-code rewrite
plugins, and fan behavior must still be checked on hardware.

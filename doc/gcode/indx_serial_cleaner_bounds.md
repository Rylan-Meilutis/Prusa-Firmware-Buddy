# INDX serial purge motion after G12

The native `G12 S30` ejection sequence finishes at cleaner-local X0 Y87.
Slicer start G-code may then use `G91` and `G1 Y-1.5 E...` to purge.
That position is beyond the printable area's X boundary; rejecting every
serial move there cancels a valid print after mesh probing has completed.

The serial linear-motion guard permits Y-only motion with both endpoints
inside cleaner-local X [-0.5, 0.5] and Y [76, 101.5] mm. Coordinates use
the saved cleaner X/Y calibration and the applied hotend offset, matching
`G750`. Commands specifying X or Z cannot use this exception. Enter and
exit the cleaner with the native `G12 S90` / `G12 S91` sequences.

Regression coverage includes the post-ejection Y87 to Y85.5 purge, its return,
and rejected lateral entry, Z motion, and out-of-lane endpoints. Hardware
verification of a complete sliced print remains necessary.

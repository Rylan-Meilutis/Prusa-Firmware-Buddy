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

## First-layer arcs

Serial G2/G3 checks use the commanded direction and sweep. Only the rightmost
and bottommost circle extrema actually traversed are checked against service
boundaries, together with both endpoints. Full circles retain full-circle
validation using the planner's endpoint-coincidence rule. Nonfinite geometry
is rejected. Short, large-radius arcs no longer fail because an unused part
of their circle overlaps the docks.

Regression coverage includes the reported G3 from X114.636 Y99.869 to
X114.644 Y101.012 with I-177.041 J1.855, reversed direction, full circles,
unsafe interior extrema, and sampled sweeps in both directions. No heap
allocation is added by this check.

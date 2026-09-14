# RME 6.10.1 port audit

The upstream 6.10.1 delta is limited to translations, welcome-screen logic,
Prusa error-code data, and the version number. It has no direct source overlap
with the RME parser, transfer, firmware-update, filament, lighting, or serial
session implementations.

## Release gates

1. Run `rme_protocol_tests`, `transfers_tests`, `connect_tests`, and
   `eeprom_unit_tests` from a 6.10.1-configured host-test build.
2. Build the complete final 6.10.1 matrix and require every advertised preset
   to remain within every linker-enforced memory region.
3. Publish only artifacts produced by that successful matrix.
4. Compare every GitHub asset digest with its local SHA-256 value.
5. Run the exhaustive RME INDX workflow mapping tests and a CORE One INDX
   link. Confirm `DIALOG QUERY` reports tool detection, slot selection,
   pickup/park errors, and each calibration FSM without allocating memory.
6. Verify the guarded slot-selection response cannot be applied outside the
   active unknown-tool selection phase, and verify parked parameterless
   temperature/fan commands remain valid.

Host tests and linker limits substantially reduce regression risk but do not
replace an on-printer smoke test of boot, RME session open, upload/resume,
serial printing, firmware status, and lighting hold.

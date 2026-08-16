# RME audit: firmware 6.6.3 and 6.8.1

This audit covers the RME serial parser, session handshake, FILE transports,
durable resume, firmware staging, crash-dump export, shared transfer ownership,
and Connect visibility on both maintained branches. It was completed before
publishing the refreshed `v6.6.3-RME` and `v6.8.1-RME` releases.

## Corrected findings

- Connection dispatch copied a 640-byte G-code line onto the Marlin task stack.
  RME dispatch is now zero-copy under its existing recursion guard.
- Text-bulk decoding allocated another 384-byte automatic payload. All upload
  and read transports now reuse the serialized static payload buffer.
- Metadata create/resume placed two full paths and a metadata record on the
  Marlin stack (more than 800 bytes). These serialized scratch objects are now
  static.
- Resume reopen, read, or prefix-hash failure could erase the durable partial
  and restart at zero. It now returns `resume_failed ... resumable=1`, restores
  line mode, releases ownership, and preserves the checkpoint.
- A malformed SHA or selection of another manifest job could delete an
  unrelated suspended upload. Only explicit cleanup, completion, or replacement
  of the same requested path removes that job.
- Metadata was truncated in place. It is now fsynced to private `.rme-tmp`
  storage and published by rename; resume accepts the complete temporary record
  after interrupted publication.
- Restored resume bytes were absent from the shared monitor. A new monitor slot
  is credited with the recovered prefix before more data is accepted.
- Multi-megabyte firmware-candidate hashing could starve thermal/watchdog work.
  The streaming hash periodically services the heater manager without reducing
  chunk size or transport window.
- Action dispatch accepted prefixes such as `ABORTgarbage`. Actions must now be
  complete tokens; malformed joined transport lines cannot alias commands.
- Crash-dump export only checked the transfer latch before a long write. It now
  owns a monitor slot for the operation. Queued `M32` and `M997` also recheck
  ownership at execution, closing the enqueue/overtake race with Connect/Link.
- `.rme-part`, `.rme-meta`, `.rme-tmp`, `.rme-old`, `FWUPD.RME`, and `FWUPD.UI`
  remain hidden from Connect and ordinary RME listings while explicit,
  manifest-driven RME resume/abort/unstage can manage them.

## Preserved protocol and performance properties

- Upload payloads remain streamed directly to FAT storage. No complete file is
  retained in RAM.
- Binary framing remains 1024 bytes with an eight-frame advertised window;
  bulk remains 384 bytes with a four-command window.
- Binary corrupt-header recovery, framed control/abort, inactivity suspension,
  committed-offset ACK/NACK behavior, SHA-256 finalization, and atomic final-file
  replacement remain enabled.
- `transfers::Monitor::instance` remains the sole cross-producer latch. RME,
  Connect, Link, crash-dump export, print start, and firmware start cannot
  intentionally overlap storage ownership.
- Ordinary `.BBF` files are not firmware candidates. Only verified
  `FWUPD.RME` plus the retained one-shot selection can report staged/armed state.

## Host requirements

Hosts must persist path, size, and SHA-256 before BEGIN; resume only an exact
manifest match; treat `resume_failed` as retryable without offset-zero fallback;
and use explicit ABORT/UNSTAGE for discard. Private sidecars must never be
inferred from Connect directory listings. See the protocol reference and serial
handler integration guide for the complete state machine.

## Regression validation

- RME parser/unit tests cover token-bounded parameters, full-width transaction
  IDs, strict numeric/path decoding, complete action tokens, binary byte-loss,
  duplicate/corrupt/oversized frames, recovery controls, and abort states.
- Connect tests cover private-file classification including `.rme-tmp`.
- Both branches are built with the canonical multi-version release command:
  `./build.py --final --versions 6.6.3 6.8.1 --jobs 15`.
- Release publication must replace the canonical version tag/release in place
  and attach only artifacts produced by that successful combined build.

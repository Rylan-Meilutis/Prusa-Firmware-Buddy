# Running firmware identity and GitHub update discovery

`@RME FILE CAPS` advertises `firmware_running=1` on supported builds.
The host may send `@RME FIRMWARE RUNNING` while idle. The reply is:

```
RME_FIRMWARE_RUNNING algorithm=app-sha256-v1 model=COREONEINDX version=6.10.1-RME size=... sha256=...
```

This SHA-256 covers application flash from the vector table through the end
of initialized-data load bytes, exactly the firmware.bin payload in the BBF.
It is calculated once per boot, only while idle, and cached in RAM. The query
does not touch USB, flash storage, or the staged-candidate state. It identifies
the application currently executing, even when USB was removed or a different
candidate was uploaded. It is not the checksum of the complete BBF container.
Resource and bootloader digest sections are within the application image.

`RME_FIRMWARE` remains the staged USB candidate status. Never use its hash,
an upload-completion event, or a version string as proof of successful flashing.

## Release gate

After building and giving BBFs their final release filenames, generate:

```
python3 utils/rme_firmware_manifest.py bbf/6.10.1 --output /tmp/rme-firmware-manifest.json
```

Upload `rme-firmware-manifest.json` alongside those exact BBFs on GitHub. Run
separately for each firmware release. Regenerate and replace the manifest when
replacing assets under an existing release tag. The script verifies embedded
BBF checksums and emits the full-file SHA-256, payload SHA-256 and size, exact
asset name, and printer variant. Never reuse metadata from another build.

RME downloads only metadata automatically; explicit selection downloads a BBF
to Pi storage with full-file checksum verification. Matching payloads need no
update prompt. Missing running identity or release manifests means unknown,
not installed or different. Downloads do not stage or flash automatically.

Validation: run `tests/unit/gui/test_rme_firmware_identity.py`, build each
supported variant, and confirm linker-bound length equals firmware.bin length.
On hardware, compare RUNNING against the manifest before/after a confirmed
flash and after a rejected flash, and verify an uploaded candidate never
changes RUNNING. Query/hash latency and camera/print UI remain hardware checks.

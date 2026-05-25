# Firmware Signing

The firmware build system can sign generated `.bbf` files with an ECDSA PEM private key.

For one-off builds, pass the key directly to the build wrapper:

```sh
python3 utils/build.py --preset coreone --bootloader yes --signing-key /path/to/private.key
```

For release builds through `build_all.sh`, set `FIRMWARE_SIGNING_KEY`:

```sh
FIRMWARE_SIGNING_KEY=/path/to/private.key ./build_all.sh
```

If `FIRMWARE_SIGNING_KEY` is not set, `build_all.sh` automatically uses the machine-local default key at `.local/firmware-signing-key.pem` when that file exists.

If no explicit or default key is available, the build still produces `.bbf` files, but they are packed with an all-zero signature.

## Bootloader Trust

Signing with a custom key does not make the firmware genuine to the stock Prusa bootloader. The bootloader verifies firmware signatures against the public key it already trusts. A `.bbf` signed with a different private key is useful for future private trust chains or custom bootloaders, but it cannot bypass the non-genuine firmware warning on an unchanged official bootloader.

To avoid that warning, the bootloader must trust the matching public key. That requires a bootloader built or provisioned with that public key, not a firmware-only change.

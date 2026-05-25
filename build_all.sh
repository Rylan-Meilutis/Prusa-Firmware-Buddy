#! /bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
default_signing_key="$script_dir/.local/firmware-signing-key.pem"

build_args=(--preset coreone,mini,xl,mk4,mk3.5 --bootloader yes)
signing_key="${FIRMWARE_SIGNING_KEY:-$default_signing_key}"

if [ -n "${FIRMWARE_SIGNING_KEY:-}" ] || [ -f "$signing_key" ]; then
    if [ ! -f "$signing_key" ]; then
        echo "Signing key not found: $signing_key" >&2
        exit 1
    fi
    build_args+=(--signing-key "$signing_key")
fi

python3 utils/build.py "${build_args[@]}"

for bbf in build/products/*_release_boot.bbf; do
    [ -e "$bbf" ] || continue
    mv "$bbf" "${bbf/_release_boot.bbf/.bbf}"
done

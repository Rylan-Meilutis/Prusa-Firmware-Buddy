#! /bin/bash
set -euo pipefail

python3 utils/build.py --preset coreone,mini,xl,mk4,mk3.5 --bootloader yes

for bbf in build/products/*_release_boot.bbf; do
    [ -e "$bbf" ] || continue
    mv "$bbf" "${bbf/_release_boot.bbf/.bbf}"
done

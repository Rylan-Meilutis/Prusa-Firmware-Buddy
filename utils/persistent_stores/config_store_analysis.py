#!/usr/bin/env python3
"""Print the config store item-size distribution from a built ELF.

Reads the compiled ``config_store_ns::CurrentStore`` layout out of the ELF's
DWARF debug info (via gdb) and expands item arrays into the individual journal
items they produce. The item set reflects the HAS_* options of whatever target
was built -- point it at ``build/<target>/firmware`` (the ELF, not firmware.bin).

Example:
    utils/persistent_stores/config_store_analysis.py build/coreone_indx_release_boot/firmware
"""

import argparse
import collections
import subprocess
import sys
import tempfile
from pathlib import Path

# gdb batch script: emit the item header size, then for each CurrentStore field
#   sizeof(field) <TAB> sizeof(DataT) <TAB> typename
# DataT is template argument 0 of every StoreItem/StoreItemArray.
GDB_SCRIPT = r"""
import gdb
gdb.execute("set pagination off")
print("HEADER\t%d" % int(gdb.parse_and_eval("sizeof(journal::Backend::ItemHeader)")))
for f in gdb.lookup_type("config_store_ns::CurrentStore").fields():
    if getattr(f, "is_base_class", False) or f.type is None:
        continue
    try:
        dsz = f.type.strip_typedefs().template_argument(0).sizeof
    except (gdb.error, RuntimeError, IndexError):
        dsz = -1
    print("FIELD\t%d\t%d\t%s" % (f.type.sizeof, dsz, f.type.name or str(f.type)))
gdb.execute("quit")
"""


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("elf",
                    type=Path,
                    help="firmware ELF (e.g. build/<target>/firmware)")
    ap.add_argument("--gdb", default="gdb", help="gdb binary to use")
    args = ap.parse_args()

    with tempfile.NamedTemporaryFile("w", suffix=".py") as fh:
        fh.write(GDB_SCRIPT)
        fh.flush()
        out = subprocess.run(
            [args.gdb, "-nx", "-batch", "-x", fh.name,
             str(args.elf)],
            capture_output=True,
            text=True).stdout

    items, hdr = [], 3
    for line in out.splitlines():
        parts = line.split("\t")
        if parts[0] == "HEADER":
            hdr = int(parts[1])
        elif parts[0] == "FIELD":
            msz, dsz, tn = int(parts[1]), int(parts[2]), parts[3]
            # Only the StoreItem family is written to the journal; skip helpers.
            # An N-element array stores N independent items, so expand by msz // dsz.
            if "StoreItem" in tn and dsz > 0 and msz % dsz == 0:
                items += [dsz] * (msz // dsz)

    if not items:
        sys.exit(
            "no journal items found (is this a firmware ELF with debug info?)")

    hist = collections.Counter(items)
    n, total_hdr = len(items), sum(s + hdr for s in items)

    print(
        f"{'data bytes':>10} | {'with header':>11} | {'count':>6} | {'total':>8}"
    )
    print("-" * 45)
    for s in sorted(hist):
        print(
            f"{s:>10} | {s + hdr:>11} | {hist[s]:>6} | {(s + hdr) * hist[s]:>8}"
        )
    print("-" * 45)
    print(f"{'total':>10} | {'':>11} | {n:>6} | {total_hdr:>8}")


if __name__ == "__main__":
    main()

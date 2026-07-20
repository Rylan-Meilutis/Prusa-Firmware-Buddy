#!/usr/bin/env python3
"""Extract the byte array from a generated Buddy font header."""

import re
import sys
from pathlib import Path


def main() -> int:
    source = Path(sys.argv[1]).read_text(encoding="utf-8")
    match = re.search(r"_data\[\]\s*=\s*\{(.*?)\};", source, re.DOTALL)
    if match is None:
        raise RuntimeError(f"font byte array not found in {sys.argv[1]}")
    Path(sys.argv[2]).write_bytes(bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", match.group(1))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

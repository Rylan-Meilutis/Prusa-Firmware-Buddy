#!/usr/bin/env python3
"""Extract the byte array from a generated Buddy font header."""

import re
import struct
import sys
from pathlib import Path


def main() -> int:
    source = Path(sys.argv[1]).read_text(encoding="utf-8")
    match = re.search(r"_data\[\]\s*=\s*\{(.*?)\};", source, re.DOTALL)
    if match is None:
        raise RuntimeError(f"font byte array not found in {sys.argv[1]}")
    raw = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", match.group(1)))
    dimensions = re.search(r"font_t\s+\w+\s*=\s*\{\s*(\d+)\s*,\s*(\d+)", source)
    if dimensions is None:
        raise RuntimeError(f"font dimensions not found in {sys.argv[1]}")
    width, height = (int(value) for value in dimensions.groups())
    # Older branches store whole padded rows; newer ones pack two 4-bit pixels
    # continuously. Infer the actual bytes-per-glyph from the initializer.
    old_bpr = re.search(r"font_t\s+\w+\s*=\s*\{\s*\d+\s*,\s*\d+\s*,\s*(\d+)\s*,", source)
    bytes_per_glyph = int(old_bpr.group(1)) * height if old_bpr else (width * height + 1) // 2
    if len(raw) % bytes_per_glyph:
        raise RuntimeError(f"font payload is not glyph-aligned in {sys.argv[1]}")

    encoded = bytearray()
    offsets = [0]
    for glyph_start in range(0, len(raw), bytes_per_glyph):
        glyph = raw[glyph_start:glyph_start + bytes_per_glyph]
        cursor = 0
        while cursor < len(glyph):
            if glyph[cursor] == 0:
                end = cursor + 1
                while end < len(glyph) and glyph[end] == 0 and end - cursor < 128:
                    end += 1
                encoded.append(0x80 | (end - cursor - 1))
            else:
                end = cursor + 1
                while end < len(glyph) and glyph[end] != 0 and end - cursor < 128:
                    end += 1
                encoded.append(end - cursor - 1)
                encoded.extend(glyph[cursor:end])
            cursor = end
        offsets.append(len(encoded))

    header = struct.pack("<HH", len(offsets) - 1, bytes_per_glyph)
    index = b"".join(struct.pack("<I", offset) for offset in offsets)
    Path(sys.argv[2]).write_bytes(header + index + encoded)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

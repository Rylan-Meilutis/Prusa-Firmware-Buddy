#!/usr/bin/env python3
"""Emit release identity metadata from final BBFs (never source/version guesses)."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def entry(path):
    data = path.read_bytes()
    if len(data) < 576:
        raise ValueError("Truncated BBF: %s" % path)
    length = int.from_bytes(data[96:100], "little")
    if not 0 < length <= len(data) - 576:
        raise ValueError("Invalid application length: %s" % path)
    if hashlib.sha256(data[96:576 + length]).digest() != data[64:96]:
        raise ValueError("BBF embedded checksum mismatch: %s" % path)
    match = re.fullmatch(r"(.+)_\d+\.\d+\.\d+-RME\.bbf", path.name)
    if not match:
        raise ValueError("Unexpected release filename: %s" % path)
    return dict(name=path.name,
                variant=match[1],
                size=len(data),
                sha256=hashlib.sha256(data).hexdigest(),
                application_size=length,
                application_sha256=hashlib.sha256(data[576:576 +
                                                       length]).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    files = sorted(args.directory.glob("*.bbf"))
    if not files:
        parser.error("No BBFs found")
    result = dict(schema=1,
                  algorithm="app-sha256-v1",
                  assets=[entry(p) for p in files])
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()

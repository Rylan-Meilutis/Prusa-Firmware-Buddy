#!/usr/bin/env python3
"""Release build wrapper.

Defaults to building the common physical-printer release set and stages BBFs in
./bbf for easy copying.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "bbf"
DEFAULT_SIGNING_KEY = PROJECT_ROOT / ".local" / "firmware-signing-key.pem"

MINI_LANGUAGE_PRESETS = [
    "mini-en-cs",
    "mini-en-de",
    "mini-en-es",
    "mini-en-fr",
    "mini-en-it",
    "mini-en-pl",
    "mini-en-ja",
    "mini-en-uk",
]

RELEASE_PRESETS = [
    "coreone",
    "mini",
    *MINI_LANGUAGE_PRESETS,
    "xl",
    "mk4",
    "mk3.5",
]

PRESET_ALIASES = {
    "all": RELEASE_PRESETS,
    "release": RELEASE_PRESETS,
    "mini-languages": MINI_LANGUAGE_PRESETS,
    "mini-lang": MINI_LANGUAGE_PRESETS,
    "mini-all": ["mini", *MINI_LANGUAGE_PRESETS],
}


def load_preset_names() -> set[str]:
    with (PROJECT_ROOT / "utils" / "presets" / "presets.json").open() as f:
        data = json.load(f)
    return {preset["name"] for preset in data["presets"]}


def split_csv(values: list[str]) -> list[str]:
    tokens: list[str] = []
    for value in values:
        tokens.extend(token.strip() for token in value.split(",") if token.strip())
    return tokens


def unique_presets(tokens: list[str], known_presets: set[str]) -> list[str]:
    selected: list[str] = []
    seen: set[str] = set()

    for token in tokens:
        expanded = PRESET_ALIASES.get(token, [token])
        for preset in expanded:
            if preset not in known_presets:
                raise SystemExit(f"Unknown preset: {preset}")
            if preset not in seen:
                selected.append(preset)
                seen.add(preset)

    return selected


def default_signing_key() -> Path | None:
    env_key = os.environ.get("FIRMWARE_SIGNING_KEY")
    if env_key:
        return Path(env_key)
    if DEFAULT_SIGNING_KEY.exists():
        return DEFAULT_SIGNING_KEY
    return None


def normalize_bbf_names(output_dir: Path) -> None:
    for bbf in output_dir.glob("*.bbf"):
        name = bbf.name
        for marker in ("_release_boot", "_release_noboot", "_release_emptyboot"):
            name = name.replace(marker, "")

        destination = bbf.with_name(name)
        if destination == bbf:
            continue
        if destination.exists():
            destination.unlink()
        bbf.rename(destination)


def keep_only_bbf_files(output_dir: Path) -> None:
    for product in output_dir.iterdir():
        if product.is_file() and product.suffix != ".bbf":
            product.unlink()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build release firmware and stage BBF files in a top-level folder."
    )
    parser.add_argument(
        "--preset",
        action="append",
        default=None,
        help=(
            "Comma-separated presets or aliases to build. Aliases: all, release, "
            "mini-languages, mini-all. Default: all."
        ),
    )
    parser.add_argument("--list-presets", action="store_true", help="List known release presets and exit.")
    parser.add_argument("--build-type", default="release", help="Build type passed to utils/build.py. Default: release.")
    parser.add_argument("--bootloader", default="yes", help="Bootloader mode passed to utils/build.py. Default: yes.")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Where BBFs are staged. Default: ./bbf.")
    parser.add_argument("--build-dir", type=Path, help="Build directory passed to utils/build.py.")
    parser.add_argument("--signing-key", type=Path, help="PEM signing key. Defaults to FIRMWARE_SIGNING_KEY or .local/firmware-signing-key.pem if present.")
    parser.add_argument("--no-signing-key", action="store_true", help="Do not pass a signing key, even if the default key exists.")
    parser.add_argument("--skip-bootstrap", action="store_true", help="Pass --skip-bootstrap to utils/build.py.")
    parser.add_argument("--store-output", action="store_true", help="Ask utils/build.py to store build logs in files.")
    parser.add_argument("--no-store-output", action="store_true", help="Print build output to the console.")
    parser.add_argument("--generate-dfu", action="store_true", help="Pass --generate-dfu to utils/build.py.")
    parser.add_argument("--final", action="store_true", help="Pass --final to utils/build.py.")
    parser.add_argument("--version-suffix", help="Pass --version-suffix to utils/build.py.")
    parser.add_argument("--version-suffix-short", help="Pass --version-suffix-short to utils/build.py.")
    parser.add_argument("--no-clean-output", action="store_true", help="Do not clear the output folder before building.")
    parser.add_argument("--dry-run", action="store_true", help="Print the build command without running it.")
    parser.add_argument(
        "-D",
        "--cmake-def",
        action="append",
        default=[],
        help="Custom CMake cache entry, passed through to utils/build.py.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    known_presets = load_preset_names()

    if args.list_presets:
        for preset in RELEASE_PRESETS:
            print(preset)
        return 0

    selected_presets = unique_presets(split_csv(args.preset or ["all"]), known_presets)
    output_dir = args.output_dir.resolve()

    if args.store_output and args.no_store_output:
        raise SystemExit("--store-output and --no-store-output are mutually exclusive")

    if not args.dry_run and not args.no_clean_output and output_dir.exists():
        shutil.rmtree(output_dir)
    if not args.dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)

    command = [
        sys.executable,
        str(PROJECT_ROOT / "utils" / "build.py"),
        "--preset",
        ",".join(selected_presets),
        "--build-type",
        args.build_type,
        "--bootloader",
        args.bootloader,
        "--products-dir",
        str(output_dir),
    ]

    if args.build_dir:
        command.extend(["--build-dir", str(args.build_dir)])
    if args.skip_bootstrap:
        command.append("--skip-bootstrap")
    if args.store_output:
        command.append("--store-output")
    if args.no_store_output:
        command.append("--no-store-output")
    if args.generate_dfu:
        command.append("--generate-dfu")
    if args.final:
        command.append("--final")
    if args.version_suffix is not None:
        command.extend(["--version-suffix", args.version_suffix])
    if args.version_suffix_short is not None:
        command.extend(["--version-suffix-short", args.version_suffix_short])

    signing_key = None if args.no_signing_key else (args.signing_key or default_signing_key())
    if signing_key:
        if not signing_key.exists():
            raise SystemExit(f"Signing key not found: {signing_key}")
        command.extend(["--signing-key", str(signing_key)])

    for definition in args.cmake_def:
        command.extend(["-D", definition])

    print("Building presets:", ",".join(selected_presets))
    print("BBF output:", output_dir)
    if args.dry_run:
        print(" ".join(str(part) for part in command))
        return 0

    result = subprocess.run(command, cwd=PROJECT_ROOT, check=False)
    if result.returncode != 0:
        return result.returncode

    normalize_bbf_names(output_dir)
    keep_only_bbf_files(output_dir)
    print(f"BBFs staged in {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

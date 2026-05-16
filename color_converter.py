#!/usr/bin/env python3
from __future__ import annotations

import argparse
import colorsys
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parent
DEFAULT_IMAGE_ROOT = REPO_ROOT / "src" / "gui" / "res"
DEFAULT_REPLACEMENT_COLOR = (0x4B, 0x2E, 0x83)

TARGET_COLORS = (
    (248, 104, 48),
    (150, 70, 43),
    (206, 123, 90),
    (253, 142, 103),
    (253, 184, 161),
    (0x24, 0x3A, 0x8F),
)

ORANGE_HUE_RANGE = (10 / 360, 45 / 360)
MIN_ORANGE_SATURATION = 0.28
MIN_ORANGE_VALUE = 0.20


def color_within_tolerance(color1: tuple[int, int, int], color2: tuple[int, int, int], tolerance: int) -> bool:
    return all(abs(a - b) <= tolerance for a, b in zip(color1, color2))


def color_is_orange(color: tuple[int, int, int]) -> bool:
    r, g, b = color
    hue, saturation, value = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    return (
        ORANGE_HUE_RANGE[0] <= hue <= ORANGE_HUE_RANGE[1]
        and saturation >= MIN_ORANGE_SATURATION
        and value >= MIN_ORANGE_VALUE
        and r > g >= b
    )


def color_matches_target(
    color: tuple[int, int, int],
    target_colors: tuple[tuple[int, int, int], ...],
    tolerance: int,
) -> bool:
    return color_is_orange(color) or any(color_within_tolerance(color, target, tolerance) for target in target_colors)


def replace_color_in_image(
    image_path: Path,
    target_colors: tuple[tuple[int, int, int], ...],
    replacement_color: tuple[int, int, int],
    tolerance: int,
    dry_run: bool,
) -> int:
    with Image.open(image_path) as img:
        rgba_img = img.convert("RGBA")
        get_pixels = getattr(rgba_img, "get_flattened_data", rgba_img.getdata)
        data = list(get_pixels())

    changed = 0
    new_data = []
    for item in data:
        if (
            item[3]
            and item[:3] != replacement_color
            and color_matches_target(item[:3], target_colors, tolerance)
        ):
            new_data.append((replacement_color[0], replacement_color[1], replacement_color[2], item[3]))
            changed += 1
        else:
            new_data.append(item)

    if changed and not dry_run:
        rgba_img.putdata(new_data)
        rgba_img.save(image_path)

    return changed


def process_images_in_folder(
    folder_path: Path,
    target_colors: tuple[tuple[int, int, int], ...],
    replacement_color: tuple[int, int, int],
    tolerance: int,
    dry_run: bool,
) -> int:
    total = 0
    for image_path in sorted(folder_path.rglob("*.png")):
        changed = replace_color_in_image(image_path, target_colors, replacement_color, tolerance, dry_run)
        if changed:
            action = "Would process" if dry_run else "Processed"
            print(f"{action}: {image_path.relative_to(REPO_ROOT)} ({changed} pixels)")
            total += 1
    return total


def parse_rgb(value: str) -> tuple[int, int, int]:
    value = value.strip().lstrip("#")
    if len(value) != 6:
        raise argparse.ArgumentTypeError("color must be a 6-digit hex value, for example #4B2E83")
    try:
        return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be a 6-digit hex value") from exc


def main() -> None:
    parser = argparse.ArgumentParser(description="Recolor Prusa orange PNG assets to the firmware accent color.")
    parser.add_argument(
        "folder",
        nargs="?",
        type=Path,
        default=DEFAULT_IMAGE_ROOT,
        help="folder to scan for PNGs, defaults to src/gui/res",
    )
    parser.add_argument(
        "--replacement",
        type=parse_rgb,
        default=DEFAULT_REPLACEMENT_COLOR,
        help="replacement color as hex, defaults to #4B2E83",
    )
    parser.add_argument("--tolerance", type=int, default=40, help="per-channel matching tolerance")
    parser.add_argument("--dry-run", action="store_true", help="report matching files without writing changes")
    args = parser.parse_args()

    folder = args.folder if args.folder.is_absolute() else REPO_ROOT / args.folder
    changed_files = process_images_in_folder(
        folder,
        TARGET_COLORS,
        args.replacement,
        args.tolerance,
        args.dry_run,
    )
    print(f"{'Matched' if args.dry_run else 'Processed'} {changed_files} PNG files under {folder.relative_to(REPO_ROOT)}")


if __name__ == "__main__":
    main()

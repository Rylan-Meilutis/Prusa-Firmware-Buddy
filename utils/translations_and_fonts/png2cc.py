#!/usr/bin/env python3

import argparse
import sys
import math
from PIL import Image


def png2font(src_img, char_w, char_h, columns, rows):
    """Convert PNG image to binary font format, returning the charset data."""

    # Convert image to RGB
    src_rgb = src_img.convert('RGB')
    src_w, src_h = src_rgb.size

    # Initialize charset data with zeros
    # Constants for 4 bits per pixel (2 pixels per byte)
    char_bpr = (char_w + 1) >> 1  # bytes per row
    char_size = char_h * char_bpr  # character size in bytes
    char_count = columns * rows  # character count
    charset_size = char_size * char_count  # charset size in bytes
    charset_data = bytearray(charset_size)

    # Process each source pixel
    src_pixels = src_rgb.load()
    for y in range(src_h):
        for x in range(src_w):
            r, g, b = src_pixels[x, y]
            luminance = (r + g + b) // 3
            opacity = 255 - luminance
            opacity = opacity >> 4

            # Calculate character code and position
            char_code = (x // char_w) + columns * (y // char_h)
            char_offs = char_code * char_size  # character offset in charset [bytes]
            char_x = x % char_w  # character pixel x-coord (0..char_w-1)
            char_y = y % char_h  # character pixel y-coord (0..char_h-1)
            char_row_offs = char_y * char_bpr  # character row offset [bytes]
            char_pix_offs = char_x >> 1  # character pixel offset [bytes]
            offs = char_offs + char_row_offs + char_pix_offs  # total offset in charset [bytes]

            # Calculate bit shift (4 bits per pixel, 2 pixels per byte)
            i = char_x % 2
            i = 4 - (i * 4)

            # Update character pixel data
            charset_data[offs] |= opacity << i

    return charset_data


def bin2cc(charset_data, dst_file, var_name, w, h, charset_enum):
    """Convert binary font data to C++ header format."""

    font_name = f'font_{var_name}_{w}x{h}'

    dst_file.write(f'#include "font_character_sets.hpp"\n')
    dst_file.write(f'constexpr uint8_t {font_name}_data[] = {{\n')

    for byte in charset_data:
        dst_file.write(f'    0x{byte:02x},\n')

    dst_file.write('};\n')
    dst_file.write(
        f'constexpr font_t {font_name} = {{ {w}, {h}, {math.ceil(w/2)}, {font_name}_data, 32, FontCharacterSet::{charset_enum} }};\n'
    )


def main():
    parser = argparse.ArgumentParser(
        description='png2cc - convert PNG image to C++ font header')

    parser.add_argument('--source',
                        dest='source',
                        required=True,
                        help='source png file name')
    parser.add_argument('--output',
                        dest='output',
                        required=True,
                        help='output C++ header file name')
    parser.add_argument('--width',
                        dest='width',
                        type=int,
                        required=True,
                        help='character width')
    parser.add_argument('--height',
                        dest='height',
                        type=int,
                        required=True,
                        help='character height')
    parser.add_argument('--columns',
                        dest='columns',
                        type=int,
                        required=True,
                        help='charset columns')
    parser.add_argument('--rows',
                        dest='rows',
                        type=int,
                        required=True,
                        help='charset rows')
    parser.add_argument('--type',
                        dest='type',
                        required=True,
                        help='font type (regular, bold, ...)')
    parser.add_argument('--charset',
                        dest='charset',
                        required=True,
                        help='charset (full, latin, ...)')

    args = parser.parse_args()

    try:
        # Load source PNG
        src_img = Image.open(args.source)

        # Convert PNG to binary font format
        charset_data = png2font(src_img, args.width, args.height, args.columns,
                                args.rows)

        # Write C++ header file
        with open(args.output, 'w') as out_file:
            bin2cc(charset_data, out_file, args.type, args.width, args.height,
                   args.charset)

        return 0
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())

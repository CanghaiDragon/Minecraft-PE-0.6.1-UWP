#!/usr/bin/env python3
"""Build compact, paged RGBA glyph atlases from a GNU Unifont .hex file.

Atlas layout: 256 code points per page, 16x16 cells, each cell 16x16 pixels.
Output paths are suitable for the game's existing images/font texture lookup.
Uses only the Python standard library so it can run on Windows without Pillow.
"""

from __future__ import annotations

import argparse
import gzip
import struct
import zlib
from pathlib import Path


PAGE_SIZE = 256
CELL = 16
ATLAS = CELL * 16


def png_chunk(kind: bytes, data: bytes) -> bytes:
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)


def write_png(path: Path, pixels: bytearray) -> None:
    # RGBA, filter type 0 on each row. Transparent background, opaque white glyphs.
    rows = b"".join(b"\0" + pixels[y * ATLAS * 4:(y + 1) * ATLAS * 4] for y in range(ATLAS))
    data = (b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", struct.pack(">2I5B", ATLAS, ATLAS, 8, 6, 0, 0, 0))
            + png_chunk(b"IDAT", zlib.compress(rows, 9))
            + png_chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def decode_glyph(hex_bitmap: str) -> tuple[int, list[int]]:
    raw = bytes.fromhex(hex_bitmap)
    if len(raw) == 16:
        width, stride = 8, 1
    elif len(raw) == 32:
        width, stride = 16, 2
    else:
        raise ValueError(f"unsupported Unifont glyph bitmap size: {len(raw)} bytes")
    rows = [int.from_bytes(raw[y * stride:(y + 1) * stride], "big") for y in range(16)]
    return width, rows


def convert(source: Path, output: Path, all_bmp_pages: bool = False) -> int:
    pages: dict[int, bytearray] = {}
    codepoints: set[int] = set()
    glyph_count = 0
    opener = gzip.open if source.suffix.lower() == ".gz" else open
    with opener(source, "rt", encoding="ascii") as stream:
        for line_number, line in enumerate(stream, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                codepoint_text, bitmap = line.split(":", 1)
                codepoint = int(codepoint_text, 16)
                width, rows = decode_glyph(bitmap)
            except (ValueError, TypeError) as exc:
                raise ValueError(f"{source}:{line_number}: invalid Unifont HEX record") from exc
            if codepoint > 0x10FFFF or 0xD800 <= codepoint <= 0xDFFF:
                continue

            page = codepoint >> 8
            pixels = pages.setdefault(page, bytearray(ATLAS * ATLAS * 4))
            slot = codepoint & 0xFF
            codepoints.add(codepoint)
            cell_x, cell_y = (slot & 15) * CELL, (slot >> 4) * CELL
            # Center 8-pixel glyphs in the fixed 16-pixel advance cell.
            left = cell_x + (CELL - width) // 2
            for row, bits in enumerate(rows):
                for col in range(width):
                    if bits & (1 << (width - 1 - col)):
                        offset = ((cell_y + row) * ATLAS + left + col) * 4
                        pixels[offset:offset + 4] = b"\xff\xff\xff\xff"
            glyph_count += 1

    output_pages = set(pages)
    if all_bmp_pages:
        output_pages.update(range(0x100))

    # Use the Unifont question mark for undefined code points. This lets the
    # renderer distinguish them visually without a separate per-page lookup
    # table, while preserving explicitly defined blank glyphs such as space.
    question_page = pages.get(ord("?") >> 8)
    if question_page is None:
        raise ValueError("Unifont source does not contain U+003F question mark")
    question_slot = ord("?") & 0xff
    question_x, question_y = (question_slot & 15) * CELL, (question_slot >> 4) * CELL
    question_rows = [
        bytes(question_page[((question_y + row) * ATLAS + question_x) * 4:
                            ((question_y + row) * ATLAS + question_x + CELL) * 4])
        for row in range(CELL)
    ]
    for page in output_pages:
        pixels = pages.setdefault(page, bytearray(ATLAS * ATLAS * 4))
        for slot in range(PAGE_SIZE):
            if (page * PAGE_SIZE + slot) in codepoints:
                continue
            cell_x, cell_y = (slot & 15) * CELL, (slot >> 4) * CELL
            for row, row_data in enumerate(question_rows):
                start = ((cell_y + row) * ATLAS + cell_x) * 4
                pixels[start:start + CELL * 4] = row_data
    for page in sorted(output_pages):
        pixels = pages.get(page, bytearray(ATLAS * ATLAS * 4))
        write_png(output / f"{page:02X}.png", pixels)
    return glyph_count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="GNU Unifont .hex or .hex.gz file")
    parser.add_argument("output", type=Path, help="destination directory for atlas PNG pages")
    parser.add_argument("--all-bmp-pages", action="store_true",
                        help="also emit empty pages so UWP can preload every BMP page")
    args = parser.parse_args()
    count = convert(args.source, args.output, args.all_bmp_pages)
    print(f"Converted {count} glyphs into {args.output}")


if __name__ == "__main__":
    main()

# Unicode font pipeline (UWP)

The game keeps its existing `images/font/default8.png` atlas for the legacy
single-byte font. GNU Unifont glyphs are generated into additional PNG pages;
the game can then resolve a Unicode scalar value without loading a TTF renderer.

## Atlas contract

- Input: a GNU Unifont `.hex` file (`CODEPOINT:BITMAP` records), optionally
  gzip-compressed as `.hex.gz`.
- Output: one transparent RGBA PNG per populated high-byte code point page,
  named `XX.png` (`XX` is the upper two hex digits of the code point). The
  current UWP package includes all 256 BMP pages so startup can preload them
  deterministically; currently empty pages are tiny transparent images.
- Each page is 256x256 pixels, arranged as 16 columns by 16 rows of 16x16
  cells. The low byte chooses the cell. Eight-pixel-wide Unifont glyphs are
  centered in their cell; sixteen-pixel glyphs fill its width. Each original
  cell is area-filtered into a centered 10x10 antialiased glyph to reduce its
  display size while retaining more stroke detail than a nearest-neighbor
  reduction.
- Output directory should be `handheld/data/images/font/unifont` so UWP's
  existing recursive image content rule packages the pages.
- Missing records stay transparent and are therefore detectable as missing
  glyphs by the eventual renderer. The source HEX file is a build input and
  does not need to ship in the app package.

Generate pages with:

```powershell
python tools/unifont_hex_to_png.py path\to\unifont.hex handheld\data\images\font\unifont
```

The packaged BMP pages were generated from GNU Unifont 18.0.01 Plane 0 HEX:

```powershell
python tools/unifont_hex_to_png.py tools\unifont-18.0.01.hex.gz handheld\data\images\font\unifont --all-bmp-pages
```

The upstream HEX archive is retained in `tools/` for reproducible generation.
The Unifont OFL 1.1 license is included at
`handheld/data/fonts/UNIFONT-OFL-1.1.txt`.

## Renderer integration requirements

Strings remain UTF-8 in `std::string`; UWP font operations decode complete
Unicode code points rather than indexing each UTF-8 byte. Drawing and measuring
share the same decoder and glyph metrics. Legacy single-byte text keeps its
current advance and atlas. Unifont BMP glyphs retain their 16x16 source atlas
cells but draw at 10x10 logical pixels with a 10-pixel advance, closer to the
legacy glyph height before Minecraft UI scaling. Newline layout continues to
use the standard font line height, and both UTF-8 and legacy `§` color control
sequences are handled.

UWP currently preloads original-client textures asynchronously and exposes a
synchronous cache-only texture API to the game renderer. Any implementation
that loads these pages must preserve that contract: page images need to be
preloaded before a draw can request them, or the texture cache must gain a
safe asynchronous/missing-page retry path. Do not synchronously decode PNGs
from the render loop.

The generated BMP pages show the Unifont question mark in code point cells
that have no source record, while explicitly defined blank glyphs remain
blank. The renderer also falls back to the legacy question mark if a page is
unavailable. The included Unifont source covers the BMP; supplementary planes
can use the same page format later, with corresponding startup preload and
assets added when those characters are needed.

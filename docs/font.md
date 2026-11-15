# The stroke font

Every letter is drawn as absolute strokes on a 0-10 unit box, y down. The
firmware then scales units to millimetres and millimetres to steps:

```
mm    = units * 0.7
steps = mm * 10
```

A glyph is a list of operations. `drawStroke(x1, y1, x2, y2)` is one line.
`drawPolyline(n, points)` is a connected run of points, drawn with the pen down
for the whole path.

```c
void drawLetterA(float off) {
    float leftX   = getLetterX(off, 0);
    float rightX  = getLetterX(off, 10);
    float topY    = getLetterY(0);
    float bottomY = getLetterY(10);
    float centerX = getLetterX(off, 5);
    float crossY  = getLetterY(5);

    drawStroke(leftX, bottomY, centerX, topY);      // left diagonal
    drawStroke(centerX, topY, rightX, bottomY);     // right diagonal
    drawStroke(getLetterX(off, 2), crossY, getLetterX(off, 8), crossY);
}
```

`getLetterX(off, u)` adds the glyph's horizontal offset; `getLetterY(u)` adds
the current line's offset. That is the entire local coordinate system.

## Metrics

| Quantity | Units | Millimetres | Steps |
| --- | --- | --- | --- |
| Glyph box | 10 | 7.0 | 70 |
| Letter spacing | 1.2 | 0.84 | 12 |
| Pitch (width + spacing) | 11.2 | 7.84 | 112 |
| Line width limit | 57.1 | 40.0 | 400 |
| Line spacing | 15 | 10.5 | 150 |
| Pen travel | 7.1 | 5.0 | 50 (up) to 0 (down) |

Five glyphs fit per line: `5 * 7.84 = 39.2 mm` against a 40 mm limit.

## Glyphs

All 27 are polygons or straight segments. Curves are approximated with 45 degree
chords, because axis-aligned motion is what the mechanism does well and
arc interpolation is what it does badly.

| glyph | segments | polylines | units drawn |
| --- | --- | --- | --- |
| `A` | 3 | 0 | 28.4 |
| `B` | 10 | 0 | 42.1 |
| `C` | 1 | 1 | 23.7 |
| `D` | 6 | 0 | 36.0 |
| `E` | 4 | 0 | 32.0 |
| `F` | 3 | 0 | 24.0 |
| `G` | 1 | 1 | 36.3 |
| `H` | 3 | 0 | 28.0 |
| `I` | 3 | 0 | 22.0 |
| `J` | 1 | 1 | 24.1 |
| `K` | 3 | 0 | 28.9 |
| `L` | 2 | 0 | 16.0 |
| `M` | 1 | 1 | 34.1 |
| `N` | 1 | 1 | 32.8 |
| `O` | 1 | 1 | 34.4 |
| `P` | 6 | 0 | 29.1 |
| `Q` | 2 | 1 | 39.4 |
| `R` | 7 | 0 | 34.9 |
| `S` | 1 | 1 | 37.5 |
| `T` | 2 | 0 | 20.0 |
| `U` | 1 | 1 | 27.7 |
| `V` | 2 | 0 | 22.4 |
| `W` | 1 | 1 | 31.7 |
| `X` | 2 | 0 | 28.3 |
| `Y` | 3 | 0 | 19.1 |
| `Z` | 3 | 0 | 34.1 |
| `space` | 0 | 0 | 0 |

`space` advances half a glyph box (50 steps) and draws nothing. `I` is the only
glyph whose width is queried specially by `getLetterWidth()`, for historical
reasons; it is also 10 units wide, so the special case has no effect on layout.

## Adding a glyph

1. Write `drawLetterX()` in `firmware/audioprinter/audioprinter.ino` using only
   `getLetterX(off, n)`, `getLetterY(n)`, `drawStroke`, and `drawPolyline`.
2. Add a `case 'X'` to the `drawLetter()` switch.
3. Rerun `python3 tools/extract_glyphs.py`.
4. Open `web/` and check the glyph in the atlas before flashing.

The extractor reports the glyph count and total operations, and warns if a
`drawPolyline` call declares a different point count than its array.

## Why the font is extracted rather than duplicated

The atlas could have redefined the letters in JavaScript. That would have been
faster and would have drifted immediately: glyph geometry changed more than any
other part of this project, including two late revisions that fixed the
orientation of `B` and the stroke order of `K` after prints came out wrong. The
extractor keeps one source of truth, so the picture in the browser is the code
that ran on the machine.

`tools/extract_glyphs.py` understands the exact expression forms used in the
sketch: a `float` variable assigned from `getLetterX` or `getLetterY`, four
arguments to `drawStroke`, and a `float points[][2]` array before a
`drawPolyline` call. It fails loudly on anything else rather than guessing.

## Limits

- Uppercase A-Z and space only.
- Fixed width; no kerning, no proportional spacing, no scaling.
- One size, 7 mm, set by `SCALE`.
- Glyphs are compiled in, so a new letter means a reflash.

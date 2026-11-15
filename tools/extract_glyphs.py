#!/usr/bin/env python3
"""Extract the hard-coded stroke font from the Arduino firmware.

The firmware defines every glyph as a sequence of drawStroke() calls and
polyline point arrays on a 0-10 coordinate grid. This script reads those
functions and emits the same geometry as data, so the browser atlas and the
firmware cannot drift apart.

Usage:
    python3 tools/extract_glyphs.py

Writes web/glyphs.js and web/glyphs.json.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIRMWARE = ROOT / "firmware" / "speech_plotter" / "speech_plotter.ino"
WEB = ROOT / "web"

FUNC_RE = re.compile(r"void drawLetter([A-Z])\(float off\)\s*\{(.*?)\n\}", re.S)
DECL_RE = re.compile(r"float\s+(\w+)\s*=\s*get(LetterX|LetterY)\(\s*(?:off\s*,\s*)?(-?\d+)\s*\)")
XY_RE = re.compile(r"get(LetterX|LetterY)\(\s*(?:off\s*,\s*)?(-?\d+)\s*\)")
STROKE_RE = re.compile(r"drawStroke\(([^;]*)\)\s*;")
POLY_DECL_RE = re.compile(r"float\s+points\s*\[\]\[2\]\s*=\s*\{(.*?)\}\s*;", re.S)
POLY_CALL_RE = re.compile(r"drawPolyline\(\s*(\d+)\s*,\s*points\s*\)\s*;")
POLY_PAIR_RE = re.compile(r"\{([^{}]*)\}")
CONST_RE = re.compile(r"#define\s+(\w+)\s+([-\d.]+)")


def split_args(text: str) -> list[str]:
    """Split on commas that are not inside parentheses."""
    parts, depth, current = [], 0, []
    for char in text:
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        if char == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def resolve(expression: str, symbols: dict[str, tuple[str, int]]) -> tuple[str, int]:
    """Turn an argument expression into ('x'|'y', unit value)."""
    expression = expression.strip()
    match = XY_RE.fullmatch(expression)
    if match:
        axis = "x" if match.group(1) == "LetterX" else "y"
        return axis, int(match.group(2))
    if expression in symbols:
        return symbols[expression]
    raise ValueError(f"cannot resolve coordinate expression: {expression!r}")


def point(x_expr: str, y_expr: str, symbols) -> list[int]:
    x_axis, x_val = resolve(x_expr, symbols)
    y_axis, y_val = resolve(y_expr, symbols)
    if x_axis != "x" or y_axis != "y":
        raise ValueError(f"swapped axes in point: {x_expr!r}, {y_expr!r}")
    return [x_val, y_val]


def parse_glyph(body: str) -> list[dict]:
    symbols: dict[str, tuple[str, int]] = {}
    for name, axis, value in DECL_RE.findall(body):
        symbols[name] = ("x" if axis == "LetterX" else "y", int(value))

    events = []
    poly_points = []
    poly_match = POLY_DECL_RE.search(body)
    if poly_match:
        for inner in POLY_PAIR_RE.findall(poly_match.group(1)):
            coords = split_args(inner)
            if len(coords) != 2:
                raise ValueError(f"unexpected point pair: {inner!r}")
            poly_points.append(point(coords[0], coords[1], symbols))

    for match in STROKE_RE.finditer(body):
        args = split_args(match.group(1))
        if len(args) != 4:
            raise ValueError(f"unexpected drawStroke arity: {match.group(0)!r}")
        x1, y1 = resolve(args[0], symbols)
        y1b, y1v = resolve(args[1], symbols)
        x2, y2 = resolve(args[2], symbols)
        y2b, y2v = resolve(args[3], symbols)
        if (x1, y1b) != ("x", "y") or (x2, y2b) != ("x", "y"):
            raise ValueError(f"axis mismatch in drawStroke: {match.group(0)!r}")
        events.append(((match.start(), 1), {"t": "line", "p0": [y1, y1v], "p1": [y2, y2v]}))

    for match in POLY_CALL_RE.finditer(body):
        declared = int(match.group(1))
        if declared != len(poly_points):
            print(
                f"  warning: drawPolyline declares {declared} points, array has {len(poly_points)}",
                file=sys.stderr,
            )
        events.append(((match.start(), 0), {"t": "path", "p": poly_points}))

    events.sort(key=lambda item: item[0])
    return [op for _, op in events]


def main() -> int:
    if not FIRMWARE.exists():
        print(f"firmware not found: {FIRMWARE}", file=sys.stderr)
        return 1

    source = FIRMWARE.read_text(encoding="utf-8", errors="replace")
    constants = {name: float(value) for name, value in CONST_RE.findall(source)}

    glyphs: dict[str, dict] = {}
    for letter, body in FUNC_RE.findall(source):
        try:
            ops = parse_glyph(body)
        except ValueError as error:
            print(f"  {letter}: {error}", file=sys.stderr)
            return 1
        strokes = sum(1 for op in ops if op["t"] == "line")
        paths = sum(1 for op in ops if op["t"] == "path")
        if strokes or paths:
            glyphs[letter] = {"width": 10, "ops": ops}

    # getLetterWidth() special cases, in font units (10 units = 100 steps).
    width_steps = {" ": 50}
    glyphs[" "] = {"width": width_steps[" "] / 10, "ops": []}

    steps_per_mm = constants["STEPS_PER_MM"]
    scale = constants["SCALE"]
    metrics = {
        "unitsPerGlyph": 10,
        "scale": scale,
        "stepsPerMm": steps_per_mm,
        "penUpSteps": constants["PEN_UP_STEPS"],
        "penDownSteps": constants["PEN_DOWN_STEPS"],
        "letterWidthSteps": constants["LETTER_WIDTH_STEPS"],
        "letterSpacingSteps": constants["LETTER_SPACING_STEPS"],
        "fullWidthSteps": constants["FULL_WIDTH_STEPS"],
        "lineSpacingSteps": constants["LINE_SPACING_STEPS"],
        "letterPitchMm": None,
        "lineSpacingMm": None,
        "maxLineWidthMm": None,
        "penTravelMm": None,
        "glyphHeightMm": None,
    }
    metrics["letterPitchMm"] = round(
        (metrics["letterWidthSteps"] / steps_per_mm) * scale
        + (metrics["letterSpacingSteps"] / steps_per_mm) * scale,
        4,
    )
    metrics["glyphHeightMm"] = round((metrics["letterWidthSteps"] / steps_per_mm) * scale, 4)
    metrics["lineSpacingMm"] = round((metrics["lineSpacingSteps"] / steps_per_mm) * scale, 4)
    metrics["maxLineWidthMm"] = round(metrics["fullWidthSteps"] / steps_per_mm, 4)
    metrics["penTravelMm"] = round(
        (metrics["penUpSteps"] - metrics["penDownSteps"]) / steps_per_mm, 4
    )
    metrics["glyphsPerLine"] = int(metrics["maxLineWidthMm"] // metrics["letterPitchMm"] + 0.0)

    payload = {
        "source": FIRMWARE.name,
        "glyphCount": len(glyphs),
        "metrics": metrics,
        "glyphs": glyphs,
    }

    WEB.mkdir(parents=True, exist_ok=True)
    (WEB / "glyphs.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    header = (
        "// Generated by tools/extract_glyphs.py from firmware/audioprinter/audioprinter.ino.\n"
        "// Do not edit by hand. Coordinates are the firmware's 0-10 unit grid, y down.\n"
    )
    js = header
    js += "const FONT_SOURCE = " + json.dumps(
        {"file": payload["source"], "glyphCount": payload["glyphCount"]}, indent=2
    ) + ";\n\n"
    js += "const FONT_METRICS = " + json.dumps(metrics, indent=2) + ";\n\n"
    js += "const GLYPHS = " + json.dumps(glyphs, indent=2) + ";\n"
    js += "\nconst GLYPH_ORDER = " + json.dumps(list(glyphs.keys())) + ";\n"
    (WEB / "glyphs.js").write_text(js, encoding="utf-8")

    total_ops = sum(len(g["ops"]) for g in glyphs.values())
    print(f"{payload['source']}: {len(glyphs)} glyphs, {total_ops} drawing ops")
    print(f"  glyph box      {metrics['glyphHeightMm']} mm")
    print(f"  letter pitch   {metrics['letterPitchMm']} mm")
    print(f"  max line width {metrics['maxLineWidthMm']} mm")
    print(f"  glyphs per line {metrics['glyphsPerLine']}")
    print(f"  pen travel     {metrics['penTravelMm']} mm")
    print(f"wrote {WEB / 'glyphs.js'} and {WEB / 'glyphs.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

// AudioPrinter stroke font atlas.
//
// Geometry comes from web/glyphs.js, which tools/extract_glyphs.py generates
// from the firmware's drawLetter*() functions. Nothing here redefines a glyph.
//
// There are two views:
//   atlas  - every glyph on its 0-10 unit grid, click one to watch the pen draw
//   layout - text laid out exactly as the firmware does, in millimetres

const PALETTE = {
  paper: "#faf8f3",
  sheet: "#ffffff",
  ink: "#1b1a17",
  grid: "#e9e4d8",
  gridStrong: "#d5cdbb",
  accent: "#b4441f",
  travel: "#bdb6a6",
  muted: "#8b8474",
};

const COLUMNS = 7;
const ATLAS_TOP = 128;
const ATLAS_BOTTOM = 96;
const SIDE = 52;
const GAP = 24;
const GLYPH_UNITS = 10;

const UNIT_SPEED = 17; // font units per second in the atlas view
const PAPER_SPEED = 26; // millimetres per second in the layout view

let view = "atlas";
let gridOn = true;
let unit = 40;
let cells = [];
let hovered = null;
let anim = null; // { program, t, origin, cell, speed }
let layout = null; // { placed, lines, pxPerMm, x, y, w, h, text }
let word = "HELLO";
let readoutCache = "";

function setup() {
  const canvas = createCanvas(windowWidth, windowHeight);
  canvas.parent(document.body);
  pixelDensity(Math.min(2, window.devicePixelRatio || 1));
  textFont("ui-sans-serif, system-ui, -apple-system, Segoe UI, sans-serif");

  document.getElementById("glyph-count").textContent = GLYPH_ORDER.length;
  bindControls();
  layoutAtlas();
  buildLayout(word);
  setReadout(PALETTE_MESSAGE.atlas);
}

function windowResized() {
  resizeCanvas(windowWidth, windowHeight);
  layoutAtlas();
  buildLayout(word);
  anim = null;
}

// ---------------------------------------------------------------- geometry

function layoutAtlas() {
  const rows = Math.ceil(GLYPH_ORDER.length / COLUMNS);
  const availableH = height - ATLAS_TOP - ATLAS_BOTTOM;
  const usableW = width - SIDE * 2 - GAP * (COLUMNS - 1);
  const cw = usableW / COLUMNS;
  const labelSpace = 34;
  const GRID_UNITS = GLYPH_UNITS + 2; // one unit of margin around the glyph box

  const unitByWidth = (cw * 0.88) / GRID_UNITS;
  const unitByHeight =
    (availableH - GAP * (rows - 1) - labelSpace * rows) / (GRID_UNITS * rows);
  unit = Math.max(4, Math.min(unitByWidth, unitByHeight));

  const rowPitch = GRID_UNITS * unit + labelSpace;
  const blockH = rows * rowPitch + GAP * (rows - 1);
  const startY = ATLAS_TOP + Math.max(0, (availableH - blockH) / 2);

  cells = GLYPH_ORDER.map((glyph, index) => {
    const col = index % COLUMNS;
    const row = Math.floor(index / COLUMNS);
    const x = SIDE + col * (cw + GAP);
    const y = startY + row * (rowPitch + GAP);
    return {
      glyph,
      x,
      y,
      w: cw,
      h: rowPitch,
      ox: x + (cw - GLYPH_UNITS * unit) / 2,
      oy: y + unit,
    };
  });
}

// Firmware drawing order, turned into pen-up travel and pen-down segments.
function buildProgram(glyph, ox, oy, scale) {
  const segments = [];
  let pen = null;

  for (const op of GLYPHS[glyph].ops) {
    const points =
      op.t === "line" ? [op.p0, op.p1] : op.p;

    const mapped = points.map(([ux, uy]) => [ox + ux * scale, oy + uy * scale]);

    if (pen && distance(pen, mapped[0]) > 0.01) {
      segments.push(makeSegment("travel", pen, mapped[0]));
    }
    for (let i = 1; i < mapped.length; i += 1) {
      segments.push(makeSegment("draw", mapped[i - 1], mapped[i]));
    }
    pen = mapped[mapped.length - 1];
  }

  return { segments, total: segments.reduce((sum, s) => sum + s.len, 0) };
}

function makeSegment(type, a, b) {
  return { type, a, b, len: distance(a, b) };
}

function distance(a, b) {
  return Math.hypot(b[0] - a[0], b[1] - a[1]);
}

function glyphDrawLength(glyph) {
  return GLYPHS[glyph].ops.reduce((sum, op) => {
    const points = op.t === "line" ? [op.p0, op.p1] : op.p;
    let len = 0;
    for (let i = 1; i < points.length; i += 1) {
      len += Math.hypot(
        points[i][0] - points[i - 1][0],
        points[i][1] - points[i - 1][1]
      );
    }
    return sum + len;
  }, 0);
}

// Mirror of the firmware: letter width, spacing, and wrap at FULL_WIDTH_STEPS.
function buildLayout(text) {
  const m = FONT_METRICS;
  const glyphMm = (m.letterWidthSteps / m.stepsPerMm) * m.scale;
  const spaceMm = (50 / m.stepsPerMm) * m.scale;
  const spacingMm = (m.letterSpacingSteps / m.stepsPerMm) * m.scale;
  const leadingMm = (m.lineSpacingSteps / m.stepsPerMm) * m.scale;
  const maxMm = m.fullWidthSteps / m.stepsPerMm;

  const clean = text
    .toUpperCase()
    .replace(/[^A-Z ]/g, "")
    .slice(0, 40);

  const placed = [];
  let x = 0;
  let y = 0;
  let lines = 1;

  for (let i = 0; i < clean.length; i += 1) {
    const glyph = clean[i];
    const w = glyph === " " ? spaceMm : glyphMm;
    placed.push({ glyph, x, y, w });
    x += w + spacingMm;
    if (x > maxMm && i < clean.length - 1) {
      x = 0;
      y += leadingMm;
      lines += 1;
    }
  }

  const margin = 7;
  const paperW = maxMm + margin * 2;
  const paperH = lines * leadingMm + margin * 1.6;

  const availableW = width - SIDE * 2;
  const availableH = height - ATLAS_TOP - ATLAS_BOTTOM;
  const pxPerMm = Math.min(availableW / paperW, availableH / paperH, 20);

  layout = {
    text: clean,
    placed,
    lines,
    pxPerMm,
    x: (width - paperW * pxPerMm) / 2,
    y: ATLAS_TOP + (availableH - paperH * pxPerMm) / 2,
    w: paperW * pxPerMm,
    h: paperH * pxPerMm,
    margin,
    leadingMm,
    glyphMm,
    maxMm,
  };
  return layout;
}

function buildLayoutProgram() {
  const m = FONT_METRICS;
  const { x, y, pxPerMm, margin, placed } = layout;
  const segments = [];
  let pen = null;

  for (const item of placed) {
    if (GLYPHS[item.glyph].ops.length === 0) continue;
    const ox = x + (margin + item.x) * pxPerMm;
    const oy = y + (margin + item.y) * pxPerMm;
    const program = buildProgram(item.glyph, ox, oy, m.scale * pxPerMm);
    if (pen && program.segments.length) {
      const start = program.segments[0].a;
      if (distance(pen, start) > 0.01) {
        segments.push(makeSegment("travel", pen, start));
      }
    }
    segments.push(...program.segments);
    if (program.segments.length) {
      pen = program.segments[program.segments.length - 1].b;
    }
  }

  return { segments, total: segments.reduce((sum, s) => sum + s.len, 0) };
}

// ---------------------------------------------------------------- drawing

function draw() {
  background(PALETTE.paper);
  if (view === "atlas") drawAtlas();
  else drawLayout();
}

function drawAtlas() {
  const labelSize = Math.max(10, Math.min(14, unit * 0.85));
  const activeCell = anim && anim.cell ? anim.cell : null;

  for (const cell of cells) {
    const isHover = cell === hovered;
    const isActive = activeCell === cell;

    if (isHover || isActive) {
      noStroke();
      fill(180, 68, 31, isActive ? 0.06 : 0.04);
      rect(cell.x - 6, cell.y - 6, cell.w + 12, cell.h + 12, 6);
    }

    if (gridOn) drawCellGrid(cell);

    if (gridOn && (isHover || isActive)) {
      stroke(PALETTE.accent);
      strokeWeight(1.4);
      point(cell.ox, cell.oy);
      noStroke();
      fill(PALETTE.accent);
      textSize(9);
      textAlign(RIGHT, BOTTOM);
      text("(0,0)", cell.ox - 4, cell.oy - 3);
    }
    const glyph = GLYPHS[cell.glyph];
    const weight = isHover || isActive ? 2.3 : 1.5;
    const colour = isHover || isActive ? PALETTE.accent : PALETTE.ink;

    if (isActive) {
      drawGlyphFaint(cell, PALETTE.gridStrong);
    } else {
      drawGlyph(cell.glyph, cell.ox, cell.oy, unit, colour, weight);
    }

    // label
    noStroke();
    fill(PALETTE.muted);
    textSize(labelSize);
    textAlign(CENTER, TOP);
    const label = cell.glyph === " " ? "space" : cell.glyph;
    text(label, cell.x + cell.w / 2, cell.oy + GLYPH_UNITS * unit + unit + 6);

    if (glyph.ops.length === 0) {
      stroke(PALETTE.gridStrong);
      strokeWeight(1);
      line(
        cell.ox + 1,
        cell.oy + GLYPH_UNITS * unit * 0.5,
        cell.ox + GLYPH_UNITS * unit - 1,
        cell.oy + GLYPH_UNITS * unit * 0.5
      );
    }
  }

  if (anim) advance(UNIT_SPEED);
}

function drawCellGrid(cell) {
  const size = GLYPH_UNITS * unit;
  const pad = unit;
  const span = size + pad * 2;
  const x0 = cell.ox - pad;
  const y0 = cell.oy - pad;

  // paper underneath the glyph box
  noStroke();
  fill(PALETTE.sheet);
  rect(cell.ox, cell.oy, size, size);

  // one unit of margin on every side, so strokes never sit on the border
  stroke(PALETTE.grid);
  strokeWeight(1);
  for (let i = 0; i <= GLYPH_UNITS + 2; i += 1) {
    const gx = x0 + i * unit;
    const gy = y0 + i * unit;
    line(gx, y0, gx, y0 + span);
    line(x0, gy, x0 + span, gy);
  }

  stroke(PALETTE.gridStrong);
  noFill();
  rect(cell.ox, cell.oy, size, size);
}

function drawGlyph(glyph, ox, oy, scale, colour, weight) {
  noFill();
  stroke(colour);
  strokeWeight(weight);
  strokeCap(ROUND);
  strokeJoin(ROUND);
  for (const op of GLYPHS[glyph].ops) {
    if (op.t === "line") {
      line(
        ox + op.p0[0] * scale,
        oy + op.p0[1] * scale,
        ox + op.p1[0] * scale,
        oy + op.p1[1] * scale
      );
    } else {
      beginShape();
      for (const [ux, uy] of op.p) vertex(ox + ux * scale, oy + uy * scale);
      endShape();
    }
  }
}

function drawGlyphFaint(cell, colour) {
  const scale = unit;
  noFill();
  stroke(colour);
  strokeWeight(1);
  for (const op of GLYPHS[cell.glyph].ops) {
    if (op.t === "line") {
      line(
        cell.ox + op.p0[0] * scale,
        cell.oy + op.p0[1] * scale,
        cell.ox + op.p1[0] * scale,
        cell.oy + op.p1[1] * scale
      );
    } else {
      beginShape();
      for (const [ux, uy] of op.p) vertex(cell.ox + ux * scale, cell.oy + uy * scale);
      endShape();
    }
  }
}

// Draw the pen program up to progress t, then advance it.
function advance(unitsPerSecond) {
  const speed = view === "atlas" ? unitsPerSecond * unit : unitsPerSecond * layout.pxPerMm;
  const dt = Math.min(deltaTime, 64) / 1000;
  anim.t = Math.min(anim.program.total, anim.t + speed * dt);
  drawProgram(anim.program, anim.t);
  if (anim.t >= anim.program.total) {
    setTimeout(() => {
      if (anim && anim.t >= anim.program.total) {
        anim = null;
        setReadout(view === "atlas" ? PALETTE_MESSAGE.atlas : paletteLayoutMessage());
      }
    }, 700);
  }
}

function drawProgram(program, progress) {
  let remaining = progress;
  let pen = null;

  for (const segment of program.segments) {
    if (remaining <= 0) break;
    const t = Math.min(1, remaining / segment.len);
    const [ax, ay] = segment.a;
    const [bx, by] = segment.b;
    const endX = ax + (bx - ax) * t;
    const endY = ay + (by - ay) * t;

    if (segment.type === "travel") {
      stroke(PALETTE.travel);
      strokeWeight(1);
      drawingContext.setLineDash([3, 4]);
      line(ax, ay, endX, endY);
      drawingContext.setLineDash([]);
    } else {
      stroke(PALETTE.accent);
      strokeWeight(2.4);
      strokeCap(ROUND);
      line(ax, ay, endX, endY);
    }

    pen = [endX, endY];
    remaining -= segment.len;
  }

  if (pen) {
    noStroke();
    fill(PALETTE.accent);
    circle(pen[0], pen[1], 7);
    fill(PALETTE.paper);
    circle(pen[0], pen[1], 2);
  }
}

function drawLayout() {
  const m = FONT_METRICS;
  const { x, y, w, h, pxPerMm, margin } = layout;

  noStroke();
  fill(PALETTE.sheet);
  rect(x, y, w, h, 2);

  // millimetre grid on the sheet
  if (gridOn) {
    stroke(PALETTE.grid);
    strokeWeight(1);
    for (let mm = 5; mm < w / pxPerMm; mm += 5) {
      const gx = x + mm * pxPerMm;
      line(gx, y + 4, gx, y + h - 4);
    }
    for (let mm = 5; mm < h / pxPerMm; mm += 5) {
      const gy = y + mm * pxPerMm;
      line(x + 4, gy, x + w - 4, gy);
    }
  }

  // the firmware's maximum line width
  const limitX = x + (margin + layout.maxMm) * pxPerMm;
  stroke(PALETTE.accent);
  strokeWeight(1);
  drawingContext.setLineDash([4, 5]);
  line(limitX, y + 6, limitX, y + h - 6);
  drawingContext.setLineDash([]);
  noStroke();
  fill(PALETTE.accent);
  textSize(10);
  textAlign(RIGHT, BOTTOM);
  text(`${layout.maxMm} mm line width`, limitX - 6, y + h - 12);

  // origin
  stroke(PALETTE.muted);
  strokeWeight(1.2);
  line(x + margin * pxPerMm - 5, y + margin * pxPerMm, x + margin * pxPerMm + 5, y + margin * pxPerMm);
  line(x + margin * pxPerMm, y + margin * pxPerMm - 5, x + margin * pxPerMm, y + margin * pxPerMm + 5);

  for (const item of layout.placed) {
    if (GLYPHS[item.glyph].ops.length === 0) continue;
    const ox = x + (margin + item.x) * pxPerMm;
    const oy = y + (margin + item.y) * pxPerMm;
    const animating = Boolean(anim);
    drawGlyph(
      item.glyph,
      ox,
      oy,
      m.scale * pxPerMm,
      animating ? PALETTE.gridStrong : PALETTE.ink,
      animating ? 1 : 1.7
    );
  }

  // caption under the sheet
  noStroke();
  fill(PALETTE.muted);
  textSize(11);
  textAlign(LEFT, TOP);
  text(
    `${layout.text || "empty"}  ·  ${layout.placed.length} characters  ·  ${layout.lines} line${
      layout.lines === 1 ? "" : "s"
    }  ·  ${m.letterPitchMm} mm pitch  ·  ${layout.leadingMm} mm leading`,
    x,
    y + h + 12
  );

  if (anim) advance(PAPER_SPEED);
}

// ---------------------------------------------------------------- interaction

function bindControls() {
  document.querySelectorAll("[data-view]").forEach((button) => {
    button.addEventListener("click", () => setView(button.dataset.view));
  });

  const composer = document.getElementById("composer");
  composer.addEventListener("submit", (event) => {
    event.preventDefault();
    const input = document.getElementById("word");
    word = input.value;
    buildLayout(word);
    anim = { program: buildLayoutProgram(), t: 0, cell: null };
    setReadout(
      `<b>${layout.text || "nothing"}</b> &middot; drawing ${layout.placed.length} characters, ${
        layout.lines
      } line${layout.lines === 1 ? "" : "s"}`
    );
    input.blur();
    updateHeadline();
  });

  setView("atlas");
}

function setView(next) {
  view = next;
  anim = null;
  hovered = null;

  document.querySelectorAll("[data-view]").forEach((button) => {
    const on = button.dataset.view === next;
    button.classList.toggle("is-active", on);
    button.setAttribute("aria-selected", String(on));
  });
  document
    .getElementById("composer")
    .classList.toggle("is-hidden", next !== "layout");

  if (next === "layout") {
    buildLayout(word);
    anim = { program: buildLayoutProgram(), t: 0, cell: null };
    setReadout(paletteLayoutMessage());
  } else {
    setReadout(PALETTE_MESSAGE.atlas);
  }
  updateHeadline();
}

function updateHeadline() {
  const headline = document.getElementById("headline");
  if (!headline) return;
  if (view === "atlas") {
    headline.innerHTML = `<span id="glyph-count">${GLYPH_ORDER.length}</span> glyphs, read from the firmware`;
  } else {
    headline.innerHTML = `<b>${layout.text || "nothing"}</b> at ${
      FONT_METRICS.letterPitchMm
    } mm pitch, printed by the firmware's own layout rules`;
  }
}

function mouseMoved() {
  if (view !== "atlas") return;
  const found = cells.find(
    (cell) =>
      mouseX >= cell.x - 6 &&
      mouseX <= cell.x + cell.w + 6 &&
      mouseY >= cell.y - 6 &&
      mouseY <= cell.y + cell.h + 6
  );
  if (found !== hovered) {
    hovered = found || null;
    if (hovered && !anim) describeGlyph(hovered.glyph);
    if (!hovered && !anim) setReadout(PALETTE_MESSAGE.atlas);
  }
}

function mousePressed() {
  if (view !== "atlas" || !hovered) return;
  const cell = hovered;
  const program = buildProgram(cell.glyph, cell.ox, cell.oy, unit);
  if (!program.segments.length) return;
  anim = { program, t: 0, cell };
  const ops = GLYPHS[cell.glyph].ops.length;
  setReadout(
    `<b>${cell.glyph === " " ? "space" : cell.glyph}</b> &middot; ${ops} stroke${
      ops === 1 ? "" : "s"
    } &middot; pen travelling`
  );
}

function keyPressed() {
  if (document.activeElement && document.activeElement.tagName === "INPUT") return;
  if (key === "g" || key === "G") gridOn = !gridOn;
  if (key === "1") setView("atlas");
  if (key === "2") setView("layout");
}

// ---------------------------------------------------------------- readout

const PALETTE_MESSAGE = {
  atlas: "Hover a glyph. Click to watch the pen draw it.",
};

function paletteLayoutMessage() {
  const m = FONT_METRICS;
  return `Paper model from firmware: ${m.glyphHeightMm} mm glyphs, ${m.letterPitchMm} mm pitch, ${m.glyphsPerLine} glyphs per line, ${m.maxLineWidthMm} mm line width, ${m.lineSpacingMm} mm leading.`;
}

function describeGlyph(glyph) {
  if (glyph === " ") {
    setReadout("<b>space</b> &middot; no strokes, advances half a glyph box");
    return;
  }
  const ops = GLYPHS[glyph].ops.length;
  const paths = GLYPHS[glyph].ops.filter((op) => op.t === "path").length;
  const len = glyphDrawLength(glyph).toFixed(1);
  const travel = (FONT_METRICS.penTravelMm || 5).toFixed(1);
  setReadout(
    `<b>${glyph}</b> &middot; ${ops} stroke${ops === 1 ? "" : "s"}${
      paths ? ` (${paths} polyline${paths === 1 ? "" : "s"})` : ""
    } &middot; ${len} units drawn &middot; ${travel} mm pen lift between strokes`
  );
}

function setReadout(html) {
  if (html === readoutCache) return;
  readoutCache = html;
  document.getElementById("readout").innerHTML = html;
}

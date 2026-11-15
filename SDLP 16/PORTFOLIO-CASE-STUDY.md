# VoiceTech AudioPrinter — portfolio case study

Working draft. Copy is written to be dropped into a portfolio page; the repo plan is at the end.

---

## Title options

1. AudioPrinter: a voice-driven pen plotter for people who cannot write by hand
2. Teaching a plotter to write what you say
3. VoiceTech AudioPrinter: speech to strokes on a CNC-style desk plotter

**One-line summary:** I led the software for a four-person SLDP team that turned spoken words into physical handwriting on a desktop 3-axis pen plotter, from the stroke font and coordinate system to the speech-to-serial pipeline.

**Role:** Coding lead. Software architecture, stroke font, motion control, speech front end, code flowcharts, integration testing.

**Team:** Fanni Miettinen (project manager), Katie Ma (3D modelling and design, secondary coding), Shih Kai Chu (circuitry), Vincy Chen (coding lead).

**Context:** EG-UY 1004 SLDP, NYU Tandon, Fall 2025. Four-person team, one semester, $75.98 materials budget.

---

## The problem

Writing by hand excludes people with limited hand mobility. Arthritis affects 18.6% of US adults; 1.1 million Americans live with Parkinson's; medication runs about $2,500 a year, and 46.6% of Parkinson's patients have household incomes below 200% of the poverty line.

The existing options each drop a requirement:

- Weighted pen, about $13: still requires grip and fine control.
- Voice to text, free: fast and accessible, but leaves nothing physical behind.
- Consumer printers, $40 to $80: print existing files, not the words you just spoke.

The team's target was a device that needs almost no hand input, produces real ink on real paper, and stays in consumer-electronics price territory.

## What we built

A desk-sized 3-axis machine modelled on the motion system of an open-frame 3D printer.

- **X axis:** pen carriage on a timing belt.
- **Y axis:** the entire paper platform on a belt and guide rods.
- **Z axis:** two lead screws with two stepper motors lifting and lowering the pen.
- **Control:** one Arduino UNO, two Adafruit Motor Shields at I2C addresses 0x60 and 0x61, four stepper motors, a 16x2 I2C LCD, an LED, and a momentary button.
- **Power:** two banks of four 9V batteries, one per shield.
- **Frame:** laser-cut birch ply with 3D-printed connectors, pen grip, button topper, and a printed company emblem.

No endstops, no encoders, no homing switches. It is an open-loop machine, and that choice shaped most of the software.

## Architecture

```
microphone
  -> Python: sounddevice capture at 16 kHz mono
  -> Google Cloud Speech-to-Text (LINEAR16)
  -> uppercase ASCII, newline terminated
  -> pyserial at 9600 baud
  -> Arduino: command parser
  -> glyph table lookup (stroke segments on a 0-10 grid)
  -> coordinate transform (SCALE 0.7, STEPS_PER_MM 10)
  -> AccelStepper over Adafruit Motor Shield onestep()
  -> 4 stepper motors
```

**A hand-authored vector font.** All 26 uppercase letters plus space are defined in firmware as line and polyline segments on a 0-10 coordinate grid, rendered into a 7 mm letter box at 0.7 scale. The paper model is 40 mm of usable line width, which fits exactly five glyphs at a 7.84 mm pitch, with 10.5 mm of leading. "HELLO" is not an arbitrary demo string: it is a full line.

**A serial command protocol.** Newline-terminated ASCII carries three kinds of traffic: text to render, bench commands (`UP`, `DOWN`, `LEFT`, `RIGHT`, `FORWARD`, `BACK`, `HOME`), and safety or diagnostic commands (`STOP` releases every motor and halts, `TESTCOORD` draws a box and crosshair to verify the coordinate frame before a print).

**A speech front end with a real operator loop.** Record, transcribe, send, with manual text entry for testing, a motor test submenu, and a confirmed emergency stop. A daemon thread tails Arduino telemetry and colour-codes it (`[PEN]`, `[DONE]`, `[CMD]`, `[ERROR]`), so the firmware's instrumented output is readable while the machine runs.

## The hard part

Nothing about the motion was self-evident, and the failure modes were physical.

- **Inverted axes.** The belt, the platform, and the two pen lifts do not share a sign convention. The final wrapper set is X normal, Y inverted, Z1 inverted, Z2 normal, with paired lift motors needing opposite directions to move together.
- **Mirrored and upside-down glyphs.** The first full alphabet rendered legibly but wrong: curves swapped ends, the tail of Q and the leg of K went the wrong way, P and R flipped. Most of the last two firmware revisions are pure geometry work on the tables, verified by printing and re-reading the paper, not by unit tests.
- **Hardware arrived late.** Voice recognition and the full motion stack were developed separately against a virtual COM port pair and a breadboard harness, then integrated once the frame and circuitry were assembled. The engineering notebook records the sequence: circuit plus code working on the bench, then the same code driving the assembled machine, then coordinate corrections on "I" and "T" to fix orientation, then the word "HELLO" printing correctly.
- **Two motor shields, four motors, limited pins.** Moving from four discrete drivers to two I2C shields freed the pin budget and made the enclosure possible, at the cost of sharing I2C addresses and step timing.

## Results

- Printed "IT" and "HELLO" under voice control, on the finished machine, on camera.
- About 15 to 18 seconds per glyph, including pen lifts and homing between words.
- Five glyphs per line maximum, 40 mm wide, uppercase only, single line before wrap.
- Legibility verified by photographing each print and correcting glyph geometry until the output read right side up, uncompressed, and unmirrored.
- Total landed cost $226.73, against a $75.98 materials budget, with the gap covered by open-lab components.

## Limitations, stated plainly

- Open loop: no endstops, no position feedback, no paper-presence or pen-contact sensing.
- Blocking motion: `moveToXY` and `moveZ` spin until the target is reached, so the firmware cannot respond mid-stroke. Safety relies on the `STOP` command, not on the machine noticing a fault.
- 26 uppercase letters and a space. No digits, punctuation, or lowercase.
- Voice path depends on a desktop, a Python environment, and a cloud API. The device is not standalone.
- One letter size, one font weight, one line width.

## What I would do next

1. Endstops and a homing routine, so power-on position is known instead of assumed.
2. Non-blocking motion with a command queue, so `STOP` works mid-stroke and speed can be raised without losing control.
3. A glyph format the machine reads at runtime (JSON or SVG path data) instead of a compiled switch statement, so the font stops being firmware.
4. Digits, punctuation, and proportional spacing.
5. Raspberry Pi on the machine, replacing the desktop and the cloud round trip with on-device speech recognition.
6. A paper-feed mechanism, so a message longer than one line is possible.

## Credits

Team project. Letter geometry was co-authored with Katie Ma; circuitry and the final circuit diagram were Shih Kai Chu's; all mechanical design and CAD were Katie Ma's; documentation and planning were Fanni Miettinen's. My work was the software architecture, the stroke font and coordinate system, the serial protocol, the speech front end, the code flowcharts, and integration testing.

---

# Repo plan

Target: a public repo a recruiter can read in two minutes, with the code actually runnable and the media curated.

```
sldp16-audioprinter/
  README.md                  # hero image, 3 paragraphs, video link, quick technical map
  LICENSE                    # decide: MIT code, CC BY-NC for docs and media
  docs/
    case-study.md            # this document, cleaned
    architecture.md          # pipeline diagram, protocol table, paper-space model
    font.md                  # glyph grid, scale maths, the 27 glyphs
    timeline.md              # 7 firmware revisions, 4 TTS revisions, 4 flowcharts
    limitations.md           # folded into case study if short
  firmware/
    audioprinter/            # Arduino sketch folder
      audioprinter.ino       # from "REAL FINALIZED /arduino_code_v5 (7).cpp"
  host/
    speech_to_plotter.py     # credential path replaced with env var
    requirements.txt         # sounddevice, numpy, google-cloud-speech, pyserial
  media/
    photos/                  # 6 final prototype views, downscaled
    cad/                     # 5 final renders, plus .f3z via release asset
    diagrams/                # final flowchart, final circuit diagram
    video/                   # link or release asset, not committed (100 MB)
  assets/                    # emblem, CAD source on a release, not in git history
```

Notes on the mechanics:

- Videos: commit stills and a poster frame, host the 89 s HELLO demo as a release asset or on YouTube, and embed it. 143 MB of MOV in git history is a mistake you cannot undo cheaply.
- The `.f3z`, `.3mf`, and `.stl` set: release asset or a small `cad-source` repo, not the main history.
- `host/speech_to_plotter.py` needs the Google credential path moved to `GOOGLE_APPLICATION_CREDENTIALS` from the environment before the first commit, and a `.gitignore` covering `*.json` service keys.
- Keep exactly one filename per artifact. The current set has `REAL FINALIZED ` (trailing space), `(4)(1).cpp`, and two competing `Code/` trees.
- `Katie_s Draft Code.docx` and `Vincy_s Draft Code.docx` are research scrapbooks rather than source. Either drop them or move them to `docs/research/` with a note, because a reviewer opening a "draft code" file and finding library documentation reads it as padding.

## Page structure

1. Hero: the isometric photo, the one-line summary, role, and four facts (team size, timeline, cost, 4 steppers).
2. The problem, three numbers, one paragraph.
3. The demo: 89 s video, unmuted, with the "HELLO" print above it.
4. How it works: the pipeline diagram, then the font grid.
5. The hard part: inverted axes and mirrored glyphs, with before/after prints if any survive.
6. Limitations and what I would do next. This section is what separates a student project from an engineering portfolio.
7. Credits, one line each, so the shared authorship is unambiguous.

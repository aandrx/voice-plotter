# Architecture

## Hardware

| Part | Detail |
| --- | --- |
| Controller | Arduino UNO |
| Motor drivers | 2x Adafruit Motor Shield V2, I2C 0x60 and 0x61 |
| Motors | 4x stepper, 200 steps/rev at 10 steps/mm |
| Display | 16x2 I2C LCD at 0x27 |
| Input | Momentary button on pin 31, `INPUT_PULLUP` |
| Indication | LED on pin 30 |
| Power | Two banks of four 9 V cells, one bank per shield |
| Frame | Laser-cut birch ply, 3D printed connectors and pen grip |

There are no endstops. The machine has no way to observe its own position.

### Axis assignment

| Axis | Motor | Shield port | Direction | Moves |
| --- | --- | --- | --- | --- |
| X | `stepperX` | bottom port 2 | normal | pen carriage, timing belt |
| Y | `stepperY` | bottom port 1 | inverted | paper platform, timing belt |
| Z1 | `stepperZ1` | top port 1 | inverted | left lead screw |
| Z2 | `stepperZ2` | top port 2 | normal | right lead screw |

The two lift motors are mechanically mirrored, so Z1 and Z2 need opposite
electrical directions to move the pen the same way. `forwardstepX` through
`backwardstepZ2` exist only to hold that mapping in one place; `AccelStepper`
animates position and speed, while the shield does the stepping.

## Software layers

```
host/plotter_host.py                       firmware/audioprinter.ino
------------------------                   -------------------------
sounddevice: 16 kHz mono                   Serial.readBytesUntil('\n')
        |                                          |
Google Speech-to-Text (LINEAR16)            command parser
        |                                     |        |
uppercase, strip                            glyph table  bench commands
        |                                     |        |
pyserial write "TEXT\n"  ----------->       coordinate transform
                                               |
pyserial read thread  <-----------          AccelStepper (4 axes)
  colour-coded telemetry                       |
                                          Adafruit Motor Shield
                                               |
                                          4 stepper motors
```

The two halves share nothing but a line of ASCII, which is why the speech path
could be developed and demonstrated before the mechanics existed. During that
period the host talked to a virtual COM port pair instead of a board.

## Serial protocol

9600 baud, newline terminated, case-insensitive by virtue of uppercasing on
arrival.

| Input | Effect |
| --- | --- |
| Any other string | Render it: look up each glyph, draw, then home |
| `STOP`, `HALT` | Release all four motors, show "EMERGENCY STOP", halt forever |
| `TESTCOORD` | Draw a 10x10 box plus crosshair to verify the coordinate frame |
| `UP` / `DOWN` | Lift or lower the pen |
| `LEFT` / `RIGHT` | Jog X by 20 mm, then reset the counters to zero |
| `FORWARD` / `BACK` | Jog Y by 20 mm, then reset the counters to zero |
| `HOME` | Lift pen, return to 0,0, reset counters |

Two details are deliberate. Bench commands end by resetting the position
counters, because the operator is expected to physically reposition the machine
between jogs. And every render replies with `DRAWING_COMPLETE` on its own line,
which is what the host watches for.

## Motion model

Font space is a 0-10 unit grid per glyph, y down.

```
mm      = units * SCALE                    SCALE = 0.7
steps   = mm * STEPS_PER_MM                STEPS_PER_MM = 10
target  = offset + units * 0.7 * 10        per axis
```

Worked example, the left diagonal of `A` from (0,10) to (5,0), drawn at offset
0 mm:

| Axis | Start | End |
| --- | --- | --- |
| X steps | 0 | 5 * 0.7 * 10 = 35 |
| Y steps | 10 * 0.7 * 10 = 70 | 0 |

`moveToXY` converts to steps, computes deltas against `currentX` / `currentY`,
then runs both axes together while either has distance to go. X and Y therefore
interpolate by sharing a loop, not by path planning.

Pen height is separate: `currentZ` is tracked in steps, `liftPen()` moves to 50
(5 mm up) and `lowerPen()` to 0. `PEN_DOWN_STEPS 0` means the paper surface is
the zero position, so the pen's mounting depth is the physical calibration.

### Paper layout

| Rule | Value | Firmware constant |
| --- | --- | --- |
| Glyph width | 100 steps = 7.0 mm | `LETTER_WIDTH_STEPS` |
| Letter spacing | 12 steps = 0.84 mm | `LETTER_SPACING_STEPS` |
| Line width limit | 400 steps = 40 mm | `FULL_WIDTH_STEPS` |
| Line spacing | 150 steps = 10.5 mm | `LINE_SPACING_STEPS` |

Advance after each glyph is `width + spacing`, so the pitch is 7.84 mm and five
glyphs fit in 40 mm. The wrap check runs after the advance, so a five-glyph line
is legal and a sixth glyph starts the next line. "HELLO" is exactly one full
line.

## Control flow

1. `setup()` starts serial, initialises the LCD, LED, and button, then brings up
   both shields. Failure prints the missing address and blinks the LED forever.
2. `loop()` waits on `Serial.available()`, reads one newline-terminated string,
   trims trailing whitespace, uppercases it, and checks the command table.
3. For text: lift the pen, move to the current cursor, then for each character
   set `globalYOffset`, call `drawLetter()`, advance the cursor, and wrap.
4. After the word: lift, show "DONE!", print `DRAWING_COMPLETE`, wait 2 s, home,
   and reset the cursor for the next message.

`drawLetter()` dispatches to 26 routines. Each routine is a handful of absolute
strokes expressed as `drawStroke(x1, y1, x2, y2)` or a point array passed to
`drawPolyline()`. The routines do not know about steps, millimetres, or the
paper; that is all in the transform helpers.

## Instrumentation

The firmware is chatty on purpose. Every motion logs target, current, and delta;
every glyph logs a boxed header with its offset and width; a heartbeat prints
`Serial.available()` every 30 s. When the mechanics arrived, the serial log was
the only way to tell a mirrored glyph from a wrong coordinate, so the logging
was not scaffolding to remove later, it was the debugging instrument.

The host's daemon thread reads those lines and prefixes them with `[PEN]`,
`[CMD]`, `[DONE]`, `[ERROR]`, or `[INFO]`, which keeps a 9600 baud stream
readable while the machine is moving.

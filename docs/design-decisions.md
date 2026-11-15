# Design decisions

The decisions that shaped the software, in the order they were forced on us.

## 1. Pen, not a stamp

The first concept was a rotating stamp on an ink pad, two axes (X and Z), one
stepper and one DC motor. Stamping means one motor per letter wheel position and
a lot of mechanical registration; a pen needs only continuous motion and a lift.
Switching to a pen removed the stamp wheel, the ink pad, and the rotation axis,
and turned the problem into 2D interpolation plus pen height.

**Cost:** letter quality now depends on pen pressure and paper flatness instead
of a fixed stamp face.

## 2. Three axes, four motors

X and Y move the pen over the page; Z lifts it. Mechanical symmetry put a lead
screw on each side of the pen carriage, so Z is two motors that must move
together. This is why the firmware has four stepper objects but only three
conceptual axes.

**Cost:** one more motor and driver than the minimum, and a synchronisation
requirement in `moveZ()`.

## 3. Two motor shields instead of four drivers

Early drafts wired four `AccelStepper` objects directly to Arduino pins
(`FULL4WIRE` on pins 2-13 and 22-25). That works, but consumes 16 pins on a board
with 20, and fitting the wiring into an enclosure became the driver for change.
Two I2C shields at 0x60 and 0x61 freed the pin budget and collapsed the harness.

**Cost:** both shields share one I2C bus, so address jumpering is a hard
requirement, and stepping now goes through `Adafruit_StepperMotor::onestep()`
rather than `AccelStepper`'s own pin stepping. That is why the firmware wraps
`onestep(FORWARD|BACKWARD, DOUBLE)` in eight tiny functions.

## 4. Hard-coded font instead of G-code

The plan was Inkscape to G-code to the machine, the way a 3D printer works. Two
things killed it: no G-code sender was available for the shield stack, and
G-code's arc and offset commands would have needed a parser and a planner we did
not have time to write. A 0-10 unit stroke font per glyph got us printing in
days.

**Cost:** the font is compiled into the firmware, so adding a glyph means
editing the sketch and reflashing, and there is no scaling, kerning, or
proportional spacing. The extraction script in `tools/` is the mitigation: the
geometry is data now, even if it lives in source form.

## 5. Speech on the host, motion on the board

The Arduino cannot do speech recognition, and the cloud API needs a network. So
speech stayed in Python and the board only ever saw text. This split is what
made the timeline survivable: voice to text could be demonstrated before the
frame was built, and the board could be tested from manual text entry.

**Cost:** the device is not standalone, and the demo depends on a laptop, an
internet connection, and a Google Cloud credential.

## 6. Develop against a virtual COM port

While the mechanics were being assembled, the host talked to a virtual serial
port pair instead of hardware. The transcripts were real, the Arduino was not.

**Cost:** timing and flow-control problems only appeared later, which is why the
host keeps a read thread with a short timeout rather than assuming the board is
quiet.

## 7. Open loop, deliberately

Endstops and encoders would have added wiring, code, and calibration time we did
not have. We chose to start every session by positioning the carriage by hand
and treating that as 0,0.

**Cost:** this is the biggest limitation in the project. A skipped step is
invisible until the print comes out wrong, and there is no recovery other than
`STOP` and starting over.

## What I would do next

1. **Endstops and homing.** Four switches and a homing routine would remove the
   "start from a known position" ritual and make power-on repeatable.
2. **Non-blocking motion.** Replace the `while (distanceToGo() != 0)` loops with
   a state machine so `STOP` works mid-stroke, the LCD can update during a print,
   and speed can rise without blocking the parser.
3. **Runtime glyph data.** Move the font out of the sketch and read it from a
   file on an SD card, the same shapes `tools/extract_glyphs.py` already emits.
4. **A planner.** Straight-line runs between stroke endpoints cause the pen to
   arrive with velocity, overshooting corners. Look-ahead and junction speed
   limits are the standard fix and would sharpen every letter.
5. **Digits, punctuation, and proportional widths**, so real messages are
   possible rather than demo words.
6. **A Raspberry Pi on the machine.** On-device speech recognition removes the
   laptop and the cloud round trip, and makes the device standalone.
7. **Paper feed.** One line of five letters is a demo, not a use case.

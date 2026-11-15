# Build log

Week-by-week from the team engineering notebook, filtered to software and system
design. Team context is included where it explains why the software changed.

## Week 5, late September: framing

First concept: a rotating letter stamp over an ink pad, two axes, three motors.
My assignment from that meeting was to research speech-to-text libraries and
anything that turned text into axis motion.

Two decisions came out of the research: text would arrive from an existing
speech API rather than an on-board module, and letter geometry would be authored
directly rather than generated from a drawing.

## Week 6, early October: three axes, G-code attempt

The design swung back to three axes after looking at open 3D printer motion
systems, which is where the belt-and-lead-screw arrangement came from. Plan at
that point: Inkscape to G-code to the machine, with GitHub as the source of
open-source snippets.

Revision 1 of the code flowchart dates from here.

## Week 7, mid October: Benchmark A

Deliverables were the preliminary CAD, circuit diagram, and code flowchart. The
flowchart assumed G-code for coordinates and a separate voice recognition
module.

Both assumptions did not survive. There was no practical G-code sender for the
shield stack, and the voice module was replaced by a laptop running an API.

## Weeks 8 to 9, late October: mechanics settle, first code

The mechanisms were fixed: X belt on the gantry, Y belt under the paper platform,
Z on two lead screws, linear rods instead of rails to cut cost.

First end-to-end test of circuit plus code: the LCD and shields came up, no motor
moved. The team's diagnosis was power, and the plan changed to two banks of four
9 V cells.

The keyboard replaced voice as the input path for the moment, because it
isolated motion bugs from speech bugs.

## Week 10, early November: Benchmark B and two shields

Test plan for the next phase:

1. Print "IT", photograph the output, check orientation and legibility, fix and
   repeat five times.
2. Repeat with "HELLO" to find the practical word limit.
3. Time a "HELLO" print, five runs.

Same week, four discrete drivers became two I2C motor shields. The full circuit
with LCD and motors was confirmed working on the bench.

Voice to text was demonstrated separately, with recognised words appearing on
the LCD, rather than integrated into the machine. That split was deliberate: it
proved the component without waiting for the mechanics.

## Week 11, mid November: assembly and debugging

The frame was assembled and the circuit moved onto it. The firmware gained its
instrumented logging here, because prints had to be diagnosed from a serial
stream while the machine was on the bench.

Revision 2 of the flowchart, plus the first Arduino revision that drove all four
motors through the shields: 736 lines, direct pin wiring and lift logic
scaffolded from library examples.

## Week 12, 17 November: "IT" prints

The machine printed "IT" under voice control on camera. Two coordinate fixes
made it legible: `I` and `T` were rendering mirrored and upside down, and both
needed corrected geometry, not corrected wiring.

That is the shape of most of the late bugs. The motion was right; the glyphs were
drawn into a coordinate frame that had the wrong sign convention on Y.

## Week 13, 30 November: "HELLO"

`H`, `E`, `L`, and `O` were added and tuned, and the machine printed "HELLO"
correctly, photographed and recorded. The team also fixed broken wiring and
retested the circuit, so a good part of that week was hardware, not code.

## Week 14 and after: submission

Final flowchart, final firmware, final host script, and the submission package.
One more firmware revision on 3 December fixed `B`'s interior points and the
stroke order of `K`, which had been wrong since the alphabet was written but
never printed.

## Code revision trail

| Date | Arduino | Host script | Notable |
| --- | --- | --- | --- |
| Nov 12 | 736 lines | 164 lines | Direct pin stepping, preliminary letters |
| Nov 17 | 1203 | 264 | Shields, LCD, full command set, telemetry |
| Nov 24 | 1203 | 268 | Stability, test menu |
| Nov 30 | 1308-1334 | 268 | Alphabet complete, five revisions in one week |
| Dec 1 | 1301 | 269 | Submission firmware, "HELLO" verified |
| Dec 3 | 1302 | 269 | `B` and `K` geometry fixes |

Only the final revisions are in this repo. Intermediate drafts were near-copies
kept for rollback, and their debug output has no portfolio value.

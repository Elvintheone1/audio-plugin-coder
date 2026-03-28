# Antilope — Reference Manual
*Manifold Research Centre*
*Saved as design reference for Gazelle*

---

## Overview

Antilope is a pseudo drum and dynamics machine, a dual pingable fully resonant filter,
with a distortion + multiFX feedback path, all controllable through a pattern recorder
with 3 cv sources switchable between morphable AD envelopes or stepped voltages.
Compatible with eurorack voltage levels (+10V/-10V) and has midi clk and sync inputs.
Has a stereo line level input for processing external sources, stereo line level out,
and HP output (up to 250 ohm impedance). Powered through USB C (2.4A).
Antilope is an analog and digital hybrid machine.

---

## FILTER [analog — red knobs]

Dual filter with common cutoff and resonance controls, a spread control (moving the
2 filters cutoffs in opposite directions), and independent input level, LP/HP crossfade
controls and attenuverters for v/oct CV in.

v/oct inputs can decently track for 4 octaves.

Independent euro level audio/ping ins and CVs for v/oct (normalised from VCF1 to VCF2)
and common resonance cv input.

The filters can self-oscillate in order to be treated as a dual VCO that can track
v/oct up to 4 octaves.

The Q pot control has been shaped in order to offer a quasi-linear control of the
decay length of the excited filters.

---

## DISTORTION [analog — black knobs]

The 2 filters get summed then into the distortion section, modeled after the
sunn o)) beta bass amplifier.

Composed of: level control, distortion, Feedback and a three band equaliser with
an emphasis on the low mid frequencies.

The level and feedback are VCAs, making them CV controllable.

2 position switch — feedback path:
- **LEFT:** Distortion output goes straight back to its input. Shapes distortion
  harmonics drastically till self oscillation.
- **RIGHT:** Distortion output goes through the FX chain, which output then goes
  back to the distortion input. Adds an analog feedback path for the digital FX
  section; EQ allows you to shape and control the color of the FX.

---

## FXs [digital — blue knobs]

Spin FV-1 FX section right after the distortion. 7 different stereo algorithms.

Dry/wet control + 3 control parameters for the selected effect.

The analog feedback path (FX out → distortion input) adds warmth and tone control.
Drive amount changes the character of the FX going up to crunchy, distorted and
broken textures.

### FX List

| Colour | Algorithm | P1 | P2 | P3 |
|:-------|:----------|:---|:---|:---|
| RED | Short-medium delay | Delay length | Digital feedback | Tone |
| GREEN | Dual pitch-shifting delay | Delay/pitch L | Delay/pitch R | Phaser/comb filtering |
| YELLOW | Three-tap delay/echo | Tap 1 length | Tap 2 length | Tap 3 length |
| BLUE | Dual ring mod + chorus | Freq L | Freq R | Chorus depth (fixed LFO) |
| PINK | Dual chorus | LFO 1 rate | LFO 2 rate (shorter range) | Depth |
| LIGHT BLUE | Reverse reverb | Pre-delay | Decay length | Damping |
| LILLA | Plate reverb | Pre-delay | Decay length | Damping |

---

## THE CORE [digital — grey knobs, white faders]

Pattern recorder — records events for 3 independent channels.

Each channel has dedicated CV and gate outputs (front panel gold nuts).
Triggered by CHERRY-MX keyboard buttons (short press).

Switchable between:
- **AD envelope mode** (left): faders control envelope lengths
- **Stepped voltages mode** (right): all 3 faders define step voltages (max 6 values,
  unlimited steps)
- **Centre position:** pattern recorder ignored — acts as mute/freeze

Envelope shape morphable between:
1. log attack / exp decay
2. exp attack / log decay
3. exp attack / exp decay
4. exp attack / log decay

Envelope speed: 20ms / 50Hz  to  8s / 0.125Hz

Per-channel independent settings: phase, shape, multiplication/division over playback speed.

In Step mode the Shape Encoder controls glide amount.

Touch/magnetic surface: cross modulations between the 3 channels.

Up to 7 patterns can be saved and recalled (core parameters only).

Pattern recorder is unquantised; external midi/eurorack clock works as reset every 4 steps.

---

## HOW TO RECORD A PATTERN

1. Set CV switches to envelope mode (left) or step mode (right)
2. Press red button → engages recording, waits for first CV button press
3. Press Play → stops and loops recording (sets buffer length)
4. Press rec again → overdub
5. Hold STOP + press PLAY → erase buffer
6. Hold CV button + press STOP → erase that channel's recordings

---

## HOW TO CHANGE SETTINGS ON CV CHANNELS

Hold desired CV trig button + turn encoder (press encoder = reset to zero).
LEDs show values in brightness. For mult/divider: left=multiply (steady LED),
right=divide (blinking LED).

---

## MIDI CLOCK

When receiving MIDI clock, automatically sets buffer to a 4/4 bar at that BPM.
Requires MIDI start message. If buffer already recorded, clips to 4/4 bar loop.
Detects MIDI cable unplug → resets to original buffer length.

## EURORACK CLOCK IN

Gate clock on .clk input → resets buffer every 4 beats.
Minimum clock speed: 4.095s.

EXT CLK has priority over MIDI when both are connected.

---

## BUTTON COMBOS CHEAT SHEET

| Combo | Action |
|:------|:-------|
| HOLD CV trig + turn ENCODER | Change CV channel settings |
| HOLD CV trig + push ENCODER | Set CV channel parameter to 0 |
| HOLD ENCODER + press CV trig | Assign external CV to channel |
| HOLD STOP + press PLAY | Erase all recorded buffers |
| HOLD STOP + press CV trig | Erase CV channel buffer |
| LONG PRESS RECORD | Enter SAVE menu |
| LONG PRESS PLAY | Enter LOAD menu |
| FX ENCODER turn (in save/load) | Change slot |
| FX ENCODER press (in save/load) | Select slot |

---

*End of reference manual.*

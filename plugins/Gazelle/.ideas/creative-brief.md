# GAZELLE — Creative Brief
*Dual-path resonant percussion engine*
*Inspired by the Manifold Research Centre Antilope*

---

## Hook

**"The feral heart of a hardware instrument — caged in your DAW."**

---

## Description

Gazelle is a software distillation of the Antilope's percussive core. A burst of shaped noise
slams into a dual self-oscillating resonant filter. The decaying ring passes through a
Sunn O)))-inspired distortion stage, then into a curated FX engine. An analog-style
feedback loop — switchable between direct and through-FX routing — closes the circle,
feeding tone and grit back into the distortion input.

The result is a percussion synthesiser that sits somewhere between a drum machine, a
resonator, and a broken analog box. It can produce clean, tonal pings; dirty, clipping
thuds; evolving, self-modulating textures; or all three at once.

---

## Signal Chain

```
[MIDI / Sidechain Trigger]
         │
         ▼
  [Noise Burst + AD Envelope]
         │
         ▼
  [Dual Resonant Filter]         ← Spread, LP/HP crossfade, self-oscillation
         │
         ▼
  [Distortion]                   ← Sunn O))) beta bass topology + 3-band EQ
         │
    ┌────┴────┐
    │         │
    │    [FX Engine]             ← 3 algorithms: Tape Delay / Ring Mod / Vintage Plate
    │         │
    └────►[Feedback path switch] ← LEFT: direct back to distortion input
              │                    RIGHT: through FX chain, back to distortion input
              │
              ▼
          [Output]
```

---

## Character

- **Percussive but tonal:** The resonant filter is the voice — it rings, decays, and can sing
- **Dirty by default:** Distortion is always in the chain; the question is how much
- **Alive through feedback:** The feedback loop is what separates this from a static drum synth
- **Three FX personalities:**
  - *Tape Delay* — gritty, saturated, slightly wobbly echoes; feedback colors on the way back
  - *Ring Mod* — metallic, inharmonic, alien; destroys the transient into frequency clots
  - *Vintage Plate* — dense, diffuse, warm; smears the ring into a wash of space
- **Pseudo-medieval, Dune-futuristic:** Black background, gold ornaments, Neve-style coloured
  buttons — it looks like it was pulled from a desert fortress control room

---

## Reference Hardware

**Manifold Research Centre — Antilope**
See `.ideas/antilope-reference.md` for the full manual.

Key elements translated into Gazelle:
| Antilope hardware | Gazelle software |
|---|---|
| Dual pingable resonant filter (analog) | Two-path SVF with Q → self-oscillation |
| Sunn O))) distortion + 3-band EQ | Soft/hard clip distortion + shelving EQ |
| Analog feedback path (direct / through FX) | Switchable feedback routing parameter |
| Spin FV-1 FX section (7 algorithms) | Curated FX engine (3 algorithms) |
| Pattern recorder + CV channels | MIDI note trigger (v2 scope: sidechain + sequencer) |

---

## Out of Scope (v1.0)

- Pattern recorder / step sequencer
- Audio input processing (trigger-only for now)
- CV / Eurorack integration

---

## Aesthetic Reference

- **Palette:** Near-black background (#0A0A0A), gold (#C8A84B) for labels and ornaments,
  deep red (#8B1A1A) for filter section buttons, cobalt blue (#1A3A8B) for FX buttons,
  matte black for distortion section
- **Typography:** Monospace or engraved-style caps, tight spacing
- **Ornaments:** Subtle geometric/arabesque gold filigree — not overwhelming, just enough
  to evoke the Antilope's hand-drawn quality
- **Buttons:** Two prominent trigger buttons styled as old PC mechanical keys
  (one per filter path, labelled I and II) — Neve-style coloured tops (red for filter I,
  blue for filter II)
- **Overall feel:** Instrument panel from a Dune ornithopter — purposeful, slightly alien,
  beautifully worn

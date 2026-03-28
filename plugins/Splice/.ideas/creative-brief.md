# Splice — Creative Brief
**Version:** v0.3 | **Date:** 2026-03-20 | **Phase:** Ideation (in progress)

---

## Hook
*Every sample contains infinite music. Splice finds it.*

---

## Elevator Pitch
Splice is a musique concrète inspired audio looper, splicer and synthesiser. A tempo-synced reel is divided into a visual grid of sections and slices, playable polyphonically via keyboard, arpeggiator, or a René-style cartesian sequencer. While it enables precise rhythmic manipulation, its focus is getting lost in the zones between sounds — the microcosm (Curtis Roads).

Inspired by Slate + Ash Cycles, Make Noise René, Monome/Kria, and OXI One MkII.

---

## Terminology
- **Reel** — the loaded audio source (tempo-synced, 4/4)
- **Section** — one of the 4 quarter-note divisions of the reel
- **Slice** — a subdivision of a section (1–16 per section)
- **Grid** — the control that sets slice count per section (integer 1–16)

---

## Default State / First Experience
User loads a reel. The interface greets them with **4 pleasingly coloured section boxes** — one per quarter note. That is it. Clean, immediate.

The Grid control subdivides each section into smaller squares (1–16). Playback begins polyphonically via MIDI. The currently playing slice brightens.

---

## The Grid

```
1 reel (1 bar, 4/4)
└── 4 sections (quarter notes, fixed)
    └── Each section: 1–16 slices (Grid control)
        Grid = 1  → 4  total slices
        Grid = 2  → 8  total slices
        Grid = 4  → 16 total slices
        Grid = 8  → 32 total slices
        Grid = 16 → 64 total slices (max)
```

**Interaction:**
- Click slice → toggle active / inactive (black = muted, skipped)
- Active slice brightens when playing (playback position indicator)
- Colors are visual differentiation — not encoding values

---

## Playback Modes

### POLY (default)
Full reel mapped chromatically across keyboard. MIDI keys trigger polyphonic playback at different pitches. Time-stretched to maintain tempo sync.

### SLICE
Sampler-style: each slice mapped to a key from C1 upward. Slices independently triggerable.

### ARP
Held keys arpeggiated rhythmically. Tempo-synced, common time divisions. **Hold** option latches active notes.

### SEQ
Separate page. Cartesian sequencer drives playback. (See SEQ page below.)

---

## Main Page Controls

### Playback Direction — two independent levels
- **Reel direction:** Fwd / Rev / Ping Pong / Random (symbol icons)
- **Slice direction:** Fwd / Rev / Ping Pong / Random (symbol icons)

### Envelope (ADSR)
One global ADSR setting. Re-triggers independently on each slice event.
- Attack / Decay / Sustain / Release

### Mod Matrix (per parameter)
Each parameter can be modulated by:
- **LFO** — waveshape (sine / tri / square / saw / random), rate (synced or free), depth
- **MIDI CC** — assignable CC, mod wheel (CC1) default

---

## SEQ Page

Activated by engaging SEQ mode. Separate page with the same visual language as the main page.

**3 cartesian sequencer layers, displayed as 3 blocks:**

| Layer | Function |
|-------|---------|
| **POS** | Position — order in which slices are triggered (René-style) |
| **VEL** | Velocity modulation per step |
| **PITCH** | Pitch modulation per step |

**Each block:**
- Subdivisible into 1–16 steps (same visual: smaller squares within the block)
- Overall step length: variable (not locked to reel bar length — enables polyrhythm)
- Preset traversal patterns: Snake, Diagonal, and others (René-style)

---

## Philosophical Anchor
While Splice allows precise rhythmic manipulation and stuttering/glitchy loop reorganisation, its focus is **getting lost in musique concrète inspired zones between sounds** — the microcosm (Curtis Roads, *Microsound*). The interface should support both precision and drift.

---

## Inspirations
| Reference | What we borrow |
|-----------|---------------|
| Slate + Ash Cycles | Grid model, section/slice hierarchy, Loop/Seq modes, visual language |
| Make Noise René | Cartesian sequencer concept, preset traversal patterns |
| Monome/Kria | Multi-lane polyrhythmic sequencer stack |
| OXI One MkII | Matriceal modulation layers |
| Musique concrète / Curtis Roads | Single-source transformation philosophy, microsound |

---

## v1 Scope

**In scope:**
- Main page: reel loading, 4 sections, Grid control, slice on/off, direction controls, ADSR, mod matrix
- Playback modes: Poly, Slice, ARP (with Hold), SEQ
- SEQ page: POS / VEL / PITCH lanes, variable step length, René patterns

**Post-v1 (future):**
- Smear — overlapping slice playback
- FX — reverb, delay
- Motion sequencer — record automation via mod wheel / XY pad

---

## Open Questions (resolve before /plan)
1. Randomization granularity — per-slice, per-section, or global offset when Gen fires?
2. Manual per-slice parameter editing — beyond on/off, can individual slices have values?
3. Loop length — strictly 1 bar, or support 2/4/8 bar loops?
4. SEQ traversal pattern list — full set of René-style patterns?
5. ARP pattern options — up / down / up-down / random / as-played?
6. Mod matrix depth — one modulation slot per parameter or multiple?

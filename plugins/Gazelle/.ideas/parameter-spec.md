# GAZELLE — Parameter Specification

Total: 18 parameters

---

## Section 1 — Envelope (Noise Burst)

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `attack` | Attack | Float | 0.5 – 50 | 2.0 | ms | Noise burst rise time |
| `decay` | Decay | Float | 5 – 8000 | 300.0 | ms | Envelope tail length |
| `env_shape` | Shape | Float | 0.0 – 1.0 | 0.5 | — | 0=log attack/exp decay → 1=exp attack/log decay |

---

## Section 2 — Dual Resonant Filter

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `cutoff` | Cutoff | Float | 20 – 20000 | 800.0 | Hz | Common cutoff for both filter paths |
| `resonance` | Resonance | Float | 0.0 – 1.0 | 0.6 | — | 0=mild, ~0.9=near-self-oscillation, 1.0=full self-osc |
| `spread` | Spread | Float | -1.0 – 1.0 | 0.0 | — | Offsets filter I up / filter II down (opposite directions) |
| `filter_mode` | Mode | Float | 0.0 – 1.0 | 0.0 | — | 0=full LP, 0.5=BP, 1.0=full HP (LP/HP crossfade) |
| `filter1_level` | Level I | Float | 0.0 – 1.0 | 1.0 | — | Filter path I input gain |
| `filter2_level` | Level II | Float | 0.0 – 1.0 | 0.8 | — | Filter path II input gain (slightly detuned by spread) |

---

## Section 3 — Distortion

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `drive` | Drive | Float | 0.0 – 1.0 | 0.4 | — | Input gain into distortion stage |
| `dist_amount` | Dist | Float | 0.0 – 1.0 | 0.3 | — | Clipping/saturation character (Sunn O))) topology) |
| `dist_feedback` | Feedback | Float | 0.0 – 1.0 | 0.0 | — | Amount fed back to distortion input |
| `feedback_path` | FB Path | Bool | false/true | false | — | false=direct (dist→dist), true=through FX (FX out→dist) |
| `eq_tilt` | EQ Tilt | Float | -1.0 – 1.0 | 0.0 | — | Tilt EQ: -1=dark (low boost/high cut), +1=bright (high boost/low cut) |

---

## Section 4 — FX Engine

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `fx_type` | FX Type | Int | 0 – 2 | 0 | — | 0=Tape Delay, 1=Ring Mod, 2=Vintage Plate |
| `fx_p1` | FX P1 | Float | 0.0 – 1.0 | 0.5 | — | Tape: delay time / Ring: carrier freq / Plate: pre-delay |
| `fx_p2` | FX P2 | Float | 0.0 – 1.0 | 0.5 | — | Tape: feedback+saturation / Ring: depth / Plate: decay |
| `fx_wet` | FX Wet | Float | 0.0 – 1.0 | 0.3 | — | Dry/wet balance for FX section |

---

## Total: 18 Parameters

---

## FX Algorithm Detail

### FX 0 — Gritty Tape Delay
| Pot | Maps to | Range |
|:----|:--------|:------|
| P1 | Delay time | 5ms – 800ms |
| P2 | Feedback + tape saturation amount | 0–1 (feedback scale 0→0.8, sat increases with P2) |

Tape character: pitch wobble (subtle LFO on delay time), HF rolloff on echoes,
soft-clip saturation that darkens as feedback rises.

### FX 1 — Ring Mod
| Pot | Maps to | Range |
|:----|:--------|:------|
| P1 | Carrier frequency | 20Hz – 4000Hz (exponential) |
| P2 | Modulation depth | 0–1 (0=dry signal, 1=fully ring-modulated) |

Stereo: L and R carrier slightly detuned (fixed ±2Hz offset) for width.

### FX 2 — Vintage Plate Reverb
| Pot | Maps to | Range |
|:----|:--------|:------|
| P1 | Pre-delay | 0ms – 80ms |
| P2 | Decay length | 0.3s – 8s |

Dattorro plate topology (same as BeadyEye). Damping fixed at mid-bright setting
appropriate for percussion. No separate damping control in v1.0.

---

## Notes

- `resonance` at 1.0 with `cutoff` in 200–2000Hz is the primary drum-synthesis sweet spot
- `spread` at ±0.3–0.5 with both paths active creates stereo width from a mono source
- `dist_feedback` + `feedback_path=true` (through FX) is the most dangerous/interesting combo
- `fx_type` is an integer choice — displayed as labelled buttons (TAPE / RING / PLATE) in UI
- EQ tilt keeps the distortion section lean (one knob instead of three-band)

# GAZELLE — DSP Architecture Specification

---

## Core Components

### 1. Trigger & Noise Burst Generator
- Receives MIDI note-on events (any note → trigger)
- MIDI velocity → optional burst amplitude scaling (v2; fixed in v1.0)
- Generates white noise via `juce::Random` or xorshift64 PRNG
- Shapes noise with a morphable AD envelope (see below)

### 2. Morphable AD Envelope
- **Attack phase:** `level += (1.0 - level) * attackCoeff` (exponential approach)
- **Decay phase:** `level *= decayCoeff` (exponential fall)
- Attack and decay coefficients derived from time constants:
  `coeff = exp(-log(9) / (timeMs * 0.001 * sampleRate))`
- `env_shape` morphs between two curve pairs via linear blend:
  - shape=0.0 → log attack (fast rise) / exp decay (long tail)
  - shape=1.0 → exp attack (slow rise) / log decay (punchy tail)
  - Implementation: blend `attackCoeff_fast` ↔ `attackCoeff_slow`, similarly for decay coeff exponent
- Envelope feeds into noise amplifier: `burstSample = noise * envelopeLevel`

### 3. Dual Resonant SVF (State Variable Filter)
**Topology:** Andy Simper TPT (Topology-Preserving Transform) SVF — chosen for:
- Correct per-sample parameter modulation without artifacts
- Stable self-oscillation (controlled by k → 0)
- Simultaneous LP/HP/BP outputs, enabling smooth crossfade

**Per-filter state:** `s1, s2` (integrator states, reset on silence)

**Per-sample computation (for each filter I and II):**
```
g   = tan(pi * fc / sampleRate)          // prewarped freq
k   = 2.0 * (1.0 - resonance^0.5)        // damping (0=self-osc, 2=overdamped)
a1  = 1.0 / (1.0 + g * (g + k))
a2  = g * a1
a3  = g * a2
v3  = x - s2                              // x = input sample
v1  = a1 * s1 + a2 * v3
v2  = s2 + a2 * s1 + a3 * v3
s1  = 2.0 * v1 - s1                      // update integrators
s2  = 2.0 * v2 - s2
lp  = v2
bp  = v1
hp  = v3 - k * v1 - v2
```

**Output crossfade (filter_mode):**
```
out = lp * (1.0 - mode) + hp * mode
// BP emerges naturally at mode=0.5 via linear blend:
// optionally: out = lp*(1-2m) + bp*(1-|1-2m|) + hp*(2m-1) clamped
```

**Frequency offsets for spread:**
```
fc_I  = cutoff * pow(2.0,  spread * 2.0)   // up to +2 octaves
fc_II = cutoff * pow(2.0, -spread * 2.0)   // down to -2 octaves
```

**Self-oscillation guard:** At resonance=1.0, k≈0.01 (not exactly 0 — prevents NaN).
Apply `tanh(s1 * 0.9)` / `tanh(s2 * 0.9)` to integrator states each sample when resonance > 0.95.

**Filter sum:**
```
filterSum = (outI * level_I + outII * level_II) * 0.5
```

### 4. Distortion Stage (Sunn O))) Beta Bass — MOSFET-inspired)
**Character:** Even-harmonic emphasis (2nd harmonic dominant), parabolic positive knee,
harder negative clip (body diode asymmetry). Inspired by MOSFET square-law saturation,
NOT tanh/sigmoid (which models BJT and produces odd harmonics).

Signal path per sample:
```
1. Input gain:   x = filterSum + feedbackSample
                 x *= 1.0 + drive * 15.0               // gain 1×→16×

2. MOSFET square-law waveshaper:
                 threshold = 1.0 - dist_amount * 0.5   // threshold narrows as dist rises
                                                        // range: 1.0 (clean) → 0.5 (fuzz)

                 if x >= 0:
                   if x < 2*threshold:
                     y = x - (x*x) / (2*threshold)     // parabolic knee — even harmonics
                   else:
                     y = threshold                      // flat saturation ceiling

                 if x < 0:
                   neg_threshold = threshold * 0.80     // harder negative clip → asymmetry
                   y = -min(-x, neg_threshold)          // body diode hard clip

3. Output normalise: y_norm = y / threshold             // keep consistent output level
                                                        // as threshold changes with dist_amount

4. Tilt EQ:      lp = lp_state * (1-lp_a) + y_norm * lp_a    // fc≈400Hz
                 hp = y_norm - lp
                 // Positive tilt → bright (boost hp, cut lp)
                 // Negative tilt → dark  (boost lp, cut hp)
                 y_eq = y_norm + eq_tilt * (0.6*hp - 0.4*lp)

5. Output:       distOutput = y_eq
```

**Why this works for the Sunn Beta Bass character:**
- Square-law positive half → dominant 2nd harmonic → warm, thick fuzz
- Asymmetric negative clip → even + odd mix → grit without harshness
- `threshold` shrinks as `dist_amount` rises → clipping happens earlier → more saturation
- No `tanh` anywhere in the dist path — that stays only in the feedback clamp

**Tilt EQ coefficients** (update at prepareToPlay or when sampleRate changes):
- LP pole: `lp_a = 1.0 - exp(-2π * 400 / sampleRate)`

### 5. Feedback Router (1-sample delay loop)
```cpp
// feedbackSample carried from previous sample
// At end of each sample:
if (!feedback_path)   // direct: dist→dist
    feedbackSample = tanh(distOutput * dist_feedback * 0.85f);
else                   // through FX: fxOut→dist
    feedbackSample = tanh(fxOutput   * dist_feedback * 0.85f);
```
`tanh` clamp is mandatory — prevents exponential blow-up at high feedback.
NaN guard: `if (!std::isfinite(feedbackSample)) feedbackSample = 0.0f;`

### 6. FX Engine

#### Algorithm 0 — Gritty Tape Delay
**State:** circular buffer (max ~1s), LFO phase, LP filter state per channel (L/R)

```
delayTime = 5ms + fx_p1 * 795ms              // 5ms → 800ms
feedbackAmt = fx_p2 * 0.80                   // max 80% feedback

// LFO wobble (tape flutter):
wobbleHz  = 1.2
wobbleMs  = 0.15 + fx_p2 * 0.25             // more sat → more wobble
lfoOffset = wobbleMs * 0.001 * sampleRate * sin(2π * wobbleHz * t)

readPos = (writePos - delayTime*sr + lfoOffset) + bufferSize) % bufferSize
tap     = linearInterp(buffer, readPos)

// HF rolloff on echo (simulates tape oxide):
lpAlpha = exp(-2π * (6000 - fx_p2*3000) / sampleRate)
lpState = lpState * lpAlpha + tap * (1-lpAlpha)   // darker as P2 rises

// Soft saturation on feedback:
fbSig   = tanh(lpState * feedbackAmt * (1 + fx_p2 * 1.5))

buffer[writePos] = distOutput + fbSig        // write
outFX = lpState * fx_wet + distOutput * (1-fx_wet)
```

#### Algorithm 1 — Ring Modulator
**State:** carrier phase L, carrier phase R (stereo detuned)

```
carrierFreq = 20.0 * pow(200.0, fx_p1)       // 20Hz → 4000Hz exponential
depth       = fx_p2                           // 0=dry, 1=fully ring-mod'd

phaseL += 2π * carrierFreq / sampleRate
phaseR += 2π * (carrierFreq + 2.0) / sampleRate   // +2Hz stereo width

ringL = distOutput * sin(phaseL)
ringR = distOutput * sin(phaseR)

outL = distOutput * (1-depth) + ringL * depth
outR = distOutput * (1-depth) + ringR * depth
outFX = (outL + outR) * 0.5 * fx_wet + distOutput * (1-fx_wet)
```
(Output stays mono pre-FX; ring mod introduces pseudo-stereo width)

#### Algorithm 2 — Vintage Plate Reverb
**Topology:** Dattorro plate — identical structure to BeadyEye (already validated)

Differences from BeadyEye implementation:
- Pre-delay: `fx_p1` → 0–80ms pre-delay buffer
- Decay: `fx_p2` → maps to `decay = 0.4 + fx_p2 * 0.45` (range 0.40–0.85)
- Damping: fixed at `damp = 0.05` (bright plate, appropriate for percussion)
- Output: `tanh(wetL * 0.5f)` / `tanh(wetR * 0.5f)`

Pre-delay buffer: separate circular buffer (max 80ms = ~4000 samples @ 48kHz)
```
preDelaySamples = fx_p1 * 0.080 * sampleRate
predelayOut = preDelayBuffer[(writePos - preDelaySamples + bufMax) % bufMax]
preDelayBuffer[writePos] = distOutput
// feed predelayOut into Dattorro input diffusers
```

---

## Processing Chain

```
MIDI note-on
     │
     ▼
[AD Envelope]──► amplitude
     │
     ▼
[White Noise Generator]
     │ (noise × envelope)
     ├──────────────────────────────────────────────────────────┐
     │                                                           │
     ▼                                                           │ feedbackSample[n-1]
[SVF Filter I  (fc_I,  Q)]──► outI × level_I                   │
[SVF Filter II (fc_II, Q)]──► outII × level_II                  │
     │                                                           │
     ▼                                                           │
[Filter Sum × 0.5]  ◄─────────────────────────────────────────┘
     │
     ▼
[Drive Gain]
     │
     ▼
[Tanh Soft Clip]
     │
     ▼
[Asymmetric Hard Clip] ─── blend by dist_amount
     │
     ▼
[Tilt EQ]
     │
     ▼──────────────────────────────── distOutput
     │                                      │
     │              ┌──────────────────────►│(direct feedback path)
     ▼              │                       │
[FX Engine]         │                       │
     │              │                       │
     ▼──────────────┤                       │
   fxOut            │(FX feedback path)     │
     │              │                       │
     └──────────────┘                       │
                    │                       │
                    ▼                       │
          feedbackSample[n] = tanh(x × dist_feedback × 0.85)
                                            │
                                            └──► [fed back to Filter Sum input]
     │
     ▼
[Output Level / Clamp]
     │
     ▼
[Stereo Out (L = R)]
```

---

## Parameter Mapping

| Parameter | Component | DSP Function |
|:----------|:----------|:-------------|
| `attack` | AD Envelope | attackCoeff = exp(-log(9)/(attack_ms * 0.001 * sr)) |
| `decay` | AD Envelope | decayCoeff = exp(-log(9)/(decay_ms * 0.001 * sr)) |
| `env_shape` | AD Envelope | blends attackCoeff_fast↔slow, decayExp |
| `cutoff` | SVF I + II | fc base frequency; spread offsets I/II |
| `resonance` | SVF I + II | k = 2*(1-res^0.5); guard at res>0.95 |
| `spread` | SVF I + II | fc_I *= 2^(spread*2), fc_II *= 2^(-spread*2) |
| `filter_mode` | SVF output | lp*(1-mode) + hp*mode crossfade |
| `filter1_level` | Filter sum | outI weight before sum |
| `filter2_level` | Filter sum | outII weight before sum |
| `drive` | Distortion | Input gain ×(1 + drive*15) |
| `dist_amount` | Distortion | Blend tanh↔asymmetric hard clip |
| `dist_feedback` | Feedback router | feedbackSample amplitude scale (×0.85 max) |
| `feedback_path` | Feedback router | Switch: distOutput vs fxOut as source |
| `eq_tilt` | Tilt EQ | Boost hp / cut lp (positive) or inverse |
| `fx_type` | FX Engine | Selects algorithm 0/1/2 |
| `fx_p1` | FX Engine | Tape: delay time / Ring: carrier freq / Plate: pre-delay |
| `fx_p2` | FX Engine | Tape: feedback+sat / Ring: depth / Plate: decay |
| `fx_wet` | FX Engine | Dry/wet mix at FX output |

---

## Memory Budget (per instance @ 48kHz)

| Buffer | Size | Bytes |
|:-------|:-----|:------|
| Tape delay (2ch) | 2 × 48000 | ~375 KB |
| Plate APF/DL (8 APFs, 4 DLs) | ~25000 total | ~100 KB |
| Pre-delay (1ch) | 4096 | ~16 KB |
| SVF state (2 filters × 2 states) | tiny | < 1 KB |
| Envelope state | tiny | < 1 KB |
| **Total** | | **~500 KB** |

Acceptable for a DAW plugin instance.

---

## Complexity Assessment

**Score: 4 / 5 (Expert)**

| Factor | Contribution |
|:-------|:-------------|
| Self-oscillating SVF with stability guard | High |
| Asymmetric distortion + tilt EQ | Moderate |
| Switchable feedback loop (1-sample delay) with stability clamp | High |
| Tape delay with LFO wobble + HF rolloff | Moderate |
| Ring modulator with stereo detuning | Low |
| Dattorro plate reverb with pre-delay | Moderate (reuse from BeadyEye) |
| Feedback path routing logic (clean, no branching artifacts) | Moderate |

**Key risks:**
1. Self-oscillating SVF → integrator state blow-up at resonance=1.0 (tanh guard required)
2. Feedback loop → exponential instability if dist_feedback×gain > 1.0 (tanh clamp required)
3. Tape delay LFO + linear interpolation → zipper noise if delay time changes abruptly (smooth time changes with 1-pole smoother)
4. Algorithm switching mid-playback → click (crossfade or only switch on silence)

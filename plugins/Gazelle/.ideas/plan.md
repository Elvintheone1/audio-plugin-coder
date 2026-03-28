# GAZELLE — Implementation Plan

**Complexity Score: 4 / 5 (Expert)**
**UI Framework: WebView (HTML/CSS/Canvas + JUCE WebBrowserComponent)**
**Strategy: Phased Implementation**

---

## Framework Rationale

WebView is the only practical choice for the described aesthetic:
- Gold arabesque ornaments, Neve-style coloured buttons, black-panel hardware feel
- Two mechanical "trigger" buttons (Filter I / II) with visual feedback
- FX type selector as labelled colour-coded buttons (not a dropdown)
- Potential for animated elements (envelope LED, VU-style output meter)

Visage could technically render this, but would require thousands of lines of custom
`draw()` C++ code to match the visual fidelity. HTML/CSS/Canvas achieves it with
far less code and is directly previewable in a browser.

Precedent: BeadyEye uses the same WebView path and has been confirmed working on macOS.

---

## JUCE Modules Required

```
juce_audio_basics        — AudioBuffer, MidiBuffer, parameter types
juce_audio_processors    — AudioProcessor, AudioProcessorValueTreeState
juce_audio_plugin_client — VST3 / AU wrapping
juce_dsp                 — Smoothed values, convolution helpers (optional)
juce_gui_extra           — WebBrowserComponent (NEEDS_WEB_BROWSER flag)
juce_gui_basics          — Component base
```

---

## Phase 2.1 — Project Scaffolding

- [ ] Create `plugins/Gazelle/CMakeLists.txt` (modelled on BeadyEye, NEEDS_WEB_BROWSER TRUE)
- [ ] Create `plugins/Gazelle/Source/PluginProcessor.h` — class skeleton + all 18 parameters declared
- [ ] Create `plugins/Gazelle/Source/PluginProcessor.cpp` — stub `processBlock`, `prepareToPlay`
- [ ] Create `plugins/Gazelle/Source/PluginEditor.h` — WebView editor skeleton
- [ ] Create `plugins/Gazelle/Source/PluginEditor.cpp` — WebView binding stub
- [ ] Create `plugins/Gazelle/Source/ui/public/index.html` — minimal placeholder
- [ ] Add Gazelle to root `CMakeLists.txt`
- [ ] Confirm `bash scripts/build-and-install-mac.sh Gazelle` compiles (empty audio, no crash)

---

## Phase 2.2 — DSP Core: Envelope + SVF

- [ ] Implement AD envelope generator in `PluginProcessor.h` (inline struct)
  - `prepare(sampleRate)`, `trigger()`, `tick()` → returns float amplitude
  - Morphable shape (blend fast/slow attack/decay coefficients)
- [ ] Implement TPT SVF in `PluginProcessor.h` (inline struct, two instances)
  - `prepare(sampleRate)`, `setParams(fc, resonance)`, `tick(x)` → struct {lp, bp, hp}
  - Integrator tanh guard at resonance > 0.95
  - NaN reset guard
- [ ] Wire MIDI note-on → `envelope.trigger()` in `processBlock`
- [ ] Wire noise generator → envelope → dual SVF → filter sum
- [ ] Bind all 9 filter/envelope parameters via APVTS listeners
- [ ] Test: standalone app triggers ping on MIDI note, rings at resonance=0.9, self-oscillates at 1.0

---

## Phase 2.3 — Distortion + Feedback Loop

- [ ] Implement distortion stage: drive gain → MOSFET square-law waveshaper (parabolic positive, hard negative) → output normalise → tilt EQ
- [ ] Implement tilt EQ (1-pole LP for shelving, coefficients from eq_tilt + sampleRate)
- [ ] Implement feedback router:
  - `feedbackSample` member variable (persists across samples)
  - Switch on `feedback_path` param: use `distOutput` or `fxOutput` as source
  - Apply `tanh(x * dist_feedback * 0.85)` clamp
  - NaN guard
- [ ] Bind `drive`, `dist_amount`, `dist_feedback`, `feedback_path`, `eq_tilt`
- [ ] Test: drive up with feedback_path=false → self-oscillation/chaos at dist_feedback > 0.8

---

## Phase 2.4 — FX Engine

- [ ] Implement Tape Delay:
  - Circular stereo buffer (max 1s @ current sample rate)
  - LFO wobble (fixed 1.2Hz, depth scaled by fx_p2)
  - Linear interpolation for fractional read
  - 1-pole LP on feedback path (fc darkens as fx_p2 rises)
  - Tanh saturation on feedback
- [ ] Implement Ring Modulator:
  - Carrier oscillator phase (L: carrierFreq, R: carrierFreq + 2Hz)
  - Depth blend dry/ring-mod output
- [ ] Implement Vintage Plate Reverb:
  - Port Dattorro implementation from BeadyEye `PluginProcessor.cpp`
  - Add pre-delay buffer (max 80ms)
  - Wire fx_p1 → pre-delay, fx_p2 → decay, fixed damping
- [ ] Implement FX algorithm switch (int param → selects active algorithm each block)
  - On switch: crossfade or zero FX state to avoid click
- [ ] Wire fx_wet dry/wet mix
- [ ] Test each algorithm independently

---

## Phase 2.5 — UI (WebView)

- [ ] Design pass: full `index.html` layout (see `/design Gazelle` phase)
  - Black background, gold ornaments/labels
  - Two mechanical trigger buttons (Filter I / II) with Neve red/blue tops
  - Knob sections: Envelope / Filter / Distortion / FX
  - FX type selector: TAPE / RING / PLATE buttons (colour-coded)
  - Output VU meter (canvas-based, simple peak/rms)
- [ ] Wire all 18 JUCE parameters to `window.__JUCE__` bindings
- [ ] Filter I / II trigger buttons → send MIDI note-on to processor (via APVTS or custom method)
- [ ] `PluginEditor.cpp`: `setSize(1000, 500)` (matching BeadyEye window dimensions)

---

## Phase 2.6 — Integration & Validation

- [ ] Full build: `bash scripts/build-and-install-mac.sh Gazelle`
- [ ] AU cache bust + standalone test
- [ ] `bash scripts/pluginval-mac.sh Gazelle` — target strictness 5, VST3 + AU pass
- [ ] Manual test matrix:
  - [ ] Resonance=1.0 → stable self-oscillation, no NaN
  - [ ] dist_feedback=0.9, feedback_path=true → feedback through plate → no blow-up
  - [ ] FX algorithm switching mid-note → no click
  - [ ] DAW host tempo change with delay running → no glitch
  - [ ] Rapid MIDI note-on → retriggering works cleanly

---

## Risk Register

| Risk | Severity | Mitigation |
|:-----|:---------|:-----------|
| SVF blow-up at resonance=1.0 | High | tanh guard on integrator states s1/s2 when res>0.95 |
| Feedback loop instability | High | tanh clamp, hard limit dist_feedback max=0.85 in APVTS |
| Tape delay zipper on time change | Medium | Smooth delayTime with juce::SmoothedValue (50ms ramp) |
| FX switch click | Medium | Crossfade 5ms or only switch when signal < -60dB |
| Dattorro plate NaN | Low | Already handled in BeadyEye — copy NaN guard |
| Ring mod phase discontinuity on retrigger | Low | Don't reset carrier phase on trigger (let it free-run) |

---

## Key Reuse From BeadyEye

| Component | Source | Notes |
|:----------|:-------|:------|
| Dattorro plate APF/DL structs | `BeadyEye/Source/PluginProcessor.h` | Copy `AllpassFilter`, `DelayLine` structs |
| Plate tank architecture | `BeadyEye/Source/PluginProcessor.cpp` `tickReverb()` | Adapt `amount` → `fx_p2` mapping |
| WebView JUCE binding pattern | `BeadyEye/Source/PluginEditor.cpp` | Same `__JUCE__` interop pattern |
| CMakeLists.txt structure | `BeadyEye/CMakeLists.txt` | Change name, PLUGIN_CODE, codes |
| `au-cache-bust-mac.sh` | Already in scripts | No changes needed |

---

## Plugin Metadata (for CMakeLists.txt)

| Field | Value |
|:------|:------|
| PLUGIN_CODE | `Gzll` |
| MANUFACTURER_CODE | `AJzz` |
| Company | Noizefield |
| Product Name | Gazelle |
| Formats | VST3, AU, Standalone |
| Window Size | 1000 × 500 |
| NEEDS_WEB_BROWSER | TRUE |

---

## Milestones

| Phase | Goal | Confidence |
|:------|:-----|:-----------|
| 2.1 Scaffolding | Compiles, empty audio | High |
| 2.2 Envelope + SVF | Produces pingable resonant sound | High |
| 2.3 Distortion + Feedback | Distorts and self-modulates | Medium (stability work needed) |
| 2.4 FX Engine | All 3 algorithms pass signal cleanly | Medium |
| 2.5 UI | Visual match to Antilope-inspired aesthetic | High (WebView precedent) |
| 2.6 Validation | pluginval pass, no NaN in stress test | Medium |

# Session Handoff — BeadyEye
**Last updated:** 2026-02-20 (session 2)
**Branch:** `feat/next-iteration`
**Machine:** macOS (original machine, Apple Silicon)

---

## Active Plugin: BeadyEye

Granular processor / effect. JUCE 8 WebView UI. 15 parameters.

### Plugin Metadata
| Field | Value |
|-------|-------|
| PLUGIN_CODE | `BdEy` |
| MANUFACTURER_CODE | `Nfld` |
| Company | Noizefield |
| Formats | VST3, AU, Standalone |
| Window size | 900×550 |

### Key File Paths
- [PluginProcessor.h](../../plugins/BeadyEye/Source/PluginProcessor.h) — APF/DL reverb structs inline
- [PluginProcessor.cpp](../../plugins/BeadyEye/Source/PluginProcessor.cpp) — all DSP
- [PluginEditor.cpp](../../plugins/BeadyEye/Source/PluginEditor.cpp) — WebView binding
- [index.html](../../plugins/BeadyEye/Source/ui/public/index.html) — full WebView UI (SVG knobs, atten rings, VU meters, grain viz)
- [CMakeLists.txt](../../plugins/BeadyEye/CMakeLists.txt)

---

## Parameters

| ID | Type | Range | Default | Notes |
|----|------|-------|---------|-------|
| `time` | float | 0–1 | 0.5 | Grain read position (0=now, 1=6s ago) |
| `size` | float | -1–1 | 0.5 | Grain length (negative = reverse) |
| `pitch` | float | -24–24 | 0.0 | Semitones |
| `shape` | float | 0–1 | 0.5 | Envelope: rect→perc→hann→pad |
| `density` | float | -1–1 | 0.0 | Grain rate (neg=reverse; 0=silent) |
| `feedback` | float | 0–1 | 0.0 | Effective max = 0.6 (scaled in processBlock) |
| `dry_wet` | float | 0–1 | 0.5 | Constant-power crossfade |
| `reverb` | float | 0–1 | 0.0 | Dattorro plate amount |
| `output_vol` | float | 0–1 | 0.75 | Smoothed output gain |
| `freeze` | bool | — | false | Stops circular buffer writes |
| `density_sync` | bool | — | false | BPM sync; falls back to free when transport stopped |
| `time_atten` | float | 0–1 | 0.0 | Per-grain time randomization |
| `size_atten` | float | 0–1 | 0.0 | Per-grain size randomization |
| `pitch_atten` | float | 0–1 | 0.0 | Per-grain pitch randomization |
| `shape_atten` | float | 0–1 | 0.0 | Per-grain shape randomization |

---

## DSP Architecture Notes

### Grain Engine
- **Pool:** 32 grains (`kMaxGrains`)
- **Buffer:** 6s circular, tanh-limited writes
- **Normalization:** `processGrains()` output tanh'd before wet/dry mix and feedback store
- **Size curve:** cubic (`absSize³`) — finer resolution near 0, large grains compressed to upper range
- **Density negative:** reverse grain direction (sizeParam negated, phaseIncrement negated)
- **Density sync:** 8 divisions (4→0.25 beats), PPQ-locked; gates on `isPlaying` — falls back to sample-count free mode when transport stopped. `currentGrainInterval` always updated from `baseInterval` (no `!densitySyncParam` guard) so free-mode fallback has a valid interval

### Feedback
- **Model:** additive — `tanh(dry + grain_normalized * fb)` written to circular buffer
- **NOT** a crossfade (was a bug: crossfade killed dry content as fb rose)
- **Effective max:** 0.75 (knob 0→1 maps to 0→0.75)
- Grain normalized with `tanh()` before being stored as `feedbackSampleL/R`

### Reverb (Dattorro Plate)
- **Input pre-scale:** `(inL + inR) * 0.5 * (1 - decay)` — normalizes tank steady-state to input amplitude
- **APF output:** `−g·in + delayed` (standard Schroeder, gain=1). Was `−g·in + delayed + g·delayed` (gain=1/(1-g) → blowup)
- **Output:** `tanh(wetL * 0.3)` — hard-bounds 8-tap sum, ~0.7× input at full wet
- **Decay:** `0.5 + amount * 0.35` (max 0.85; was 0.9 → extreme tails)
- **Damping:** steeper above 0.75: `damp = 0.0005 + 0.75*0.03 + (amount-0.75)*0.8`

---

## Build Commands

```bash
# Full build + install + AU cache bust
bash scripts/build-and-install-mac.sh BeadyEye

# Quick test (no DAW)
open "build/plugins/BeadyEye/BeadyEye_artefacts/Release/Standalone/BeadyEye.app"

# Validate
bash scripts/pluginval-mac.sh BeadyEye

# AU cache bust only (after manual install)
bash scripts/au-cache-bust-mac.sh BeadyEye
```

---

## Current Status

### Fixed (Sessions 1 & 2)
- [x] APF allpass gain bug → 100dB reverb blowup (was `−g·in + delayed + g·delayed`, DC gain 4×)
- [x] Feedback not working → was crossfade, now additive `tanh(dry + grain*fb)`
- [x] Reverb overload → tank pre-scale `(1-decay)` + `tanh(wetL*0.3)` output limiter
- [x] Reverb extreme tails → decay max reduced `0.9→0.85` (`0.5 + amount*0.35`)
- [x] Grain output normalization (tanh before wet/dry and feedback paths)
- [x] Freeze stops when DAW transport stops → `currentGrainInterval` now always updated (removed `!densitySyncParam` guard), free-mode fallback has valid interval
- [x] Feedback effective max `0.6→0.75`
- [x] Size curve quadratic→cubic (finer resolution near 0)
- [x] macOS infrastructure: `setup-mac.sh`, `system-check-mac.sh`, `pluginval-mac.sh`, `au-cache-bust-mac.sh`, `macos-build-protocols.md`

### Pending / Next Session
- [ ] **Verify all DSP fixes** feel correct (user ended session before testing latest build)
- [ ] **UI overhaul** (deferred until DSP confirmed stable):
  - Shift+click to enter atten edit mode (replace outer-ring drag; too fiddly)
  - Narrower window width
  - Time / Size / Shape knobs clustered together (left group)
  - Grain visualization: arc of dots behind density knob (background layer)
  - Output section (FDBK / MIX / REVERB / OUTPUT) larger, moved up
  - Fix knob indicator pivot alignment
- [ ] pluginval run to confirm AU validation passes
- [ ] Commit tag when DSP is confirmed stable

---

## Known Issues / Watch Points
- Float comparison warning on `currentSlot != lastSyncSlot` (line ~499) — harmless by design (value comes from `std::floor`)
- `density_sync` at density=0 returns `interval=999999` → silent (correct behavior)
- AU cache must be busted after every install on macOS; build script does this automatically
- If reverb still feels unstable after session 2 build, consider adding LFO modulation to tank APF delay lengths (prevents metallic comb resonances — standard Dattorro fix)

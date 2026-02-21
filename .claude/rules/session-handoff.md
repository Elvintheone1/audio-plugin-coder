# Session Handoff — BeadyEye
**Last updated:** 2026-02-21 (session 3 — build env resolved)
**Branch:** `feat/next-iteration`
**Machine:** macOS (second machine, Apple Silicon — now equivalent to machine 1)

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

### Build Script: Multi-Machine Generator Auto-Detection
`build-and-install-mac.sh` picks the CMake generator automatically — no hardcoded paths or generators:

| Priority | Generator | When used |
|----------|-----------|-----------|
| 1st | **Ninja** | `ninja` binary found in PATH (fast, works on all machines) |
| 2nd | **Xcode** | `xcodebuild -version` exits 0 (healthy Xcode install, machine 1) |
| 3rd | **Unix Makefiles** | xcodebuild crashes (Xcode 26 beta on machine 2) |

**Architecture:** defaults to `uname -m` (native). Override for universal binary:
```bash
ARCHS="arm64;x86_64" bash scripts/build-and-install-mac.sh BeadyEye
```

**Cache invalidation:** if a cached build used a different generator, the script wipes `CMakeCache.txt` + `CMakeFiles/` automatically before reconfiguring.

**Xcode 26.2 (17C52) note:** This is the stable release — xcodebuild works. The earlier DVTDownloads crash was a beta-only issue. Both machines now have Ninja installed, so the Ninja path is used everywhere.

---

## Current Status

### Fixed (Sessions 1–3)
- [x] APF allpass gain bug → 100dB reverb blowup (was `−g·in + delayed + g·delayed`, DC gain 4×)
- [x] Feedback not working → was crossfade, now additive `tanh(dry + grain*fb)`
- [x] Reverb overload → tank pre-scale `(1-decay)` + `tanh(wetL*0.3)` output limiter
- [x] Reverb extreme tails → decay max reduced `0.9→0.85` (`0.5 + amount*0.35`)
- [x] Grain output normalization (tanh before wet/dry and feedback paths)
- [x] Freeze stops when DAW transport stops → `currentGrainInterval` now always updated
- [x] Feedback effective max `0.6→0.75`
- [x] Size curve quadratic→cubic (finer resolution near 0)
- [x] macOS infrastructure: `setup-mac.sh`, `system-check-mac.sh`, `pluginval-mac.sh`, `au-cache-bust-mac.sh`
- [x] **Reverb LFO modulation** (session 3): Added independent LFOs (0.5/0.565 Hz, ±12 samples) to tank APFs [4] and [6]. Breaks up metallic fixed-resonance ringing = "noisy" complaint fixed
- [x] **Reverb damping overhaul** (session 3): Was `damp=0.023` at amount=0.75 (near-zero). Now `damp = 0.15 + amount * 0.55` → range [0.15, 0.70]. Natural plate warmth at all settings
- [x] **Reverb output level** (session 3): `tanh(wet * 0.3f)` → `tanh(wet * 0.5f)`. Was too quiet (−10 dB) forcing users to push reverb higher → longer tails → sounded like feedback
- [x] **UI overhaul** (session 3):
  - Window: 900→800px wide
  - Left cluster: TIME / SIZE / SHAPE grouped at left (120/215/310px)
  - DENSITY hero: 440px (slightly right of center)
  - PITCH: 660px (right)
  - Grain viz: arc of 16 animated dots behind density knob (SVG, z-index 1, radius 80px)
  - Output section: 52→64px knobs, moved up (y=378 from y=440)
  - Atten interaction: **Shift+click** toggles atten edit mode (replaces outer-ring drag). Value display shows `R XX%` in cyan when active. Click outside to exit
  - Knob indicators: start 7px from center (not at exact center) — cleaner pivot look
  - Subtle section divider line between left cluster and density hero
- [x] **Build script multi-machine proof** (session 3): Generator auto-detection — Ninja→Xcode→Unix Makefiles. Architecture defaults to native `uname -m`. Cache invalidated when generator changes. No hardcoded paths or generator names.
- [x] **Machine 2 build env** (session 3): Ninja 1.13.2 installed. Xcode 26.2 stable (17C52) confirmed working. Both machines now: Ninja (preferred) + healthy xcodebuild (fallback).

### Pending / Next Session
- [ ] **Verify session 3 DSP/UI** in Standalone (user to test)
- [ ] pluginval run to confirm AU validation passes
- [ ] Consider LFO modulation depth tuning (currently ±12 samples — adjust if reverb still sounds slightly chorused)
- [ ] Universal binary (arm64+x86_64): needs `sudo xcodebuild -runFirstLaunch` to fix Xcode 26 framework issue, or install stable Xcode
- [ ] Commit tag when DSP confirmed stable

---

## Known Issues / Watch Points
- Float comparison warning on `currentSlot != lastSyncSlot` (line ~499) — harmless by design (value comes from `std::floor`)
- `density_sync` at density=0 returns `interval=999999` → silent (correct behavior)
- AU cache must be busted after every install on macOS; build script does this automatically
- Universal binary: use `ARCHS="arm64;x86_64" bash scripts/build-and-install-mac.sh BeadyEye` on machine 1 (Ninja or Xcode generator). Machine 2's Unix Makefiles path also supports this flag
- If reverb LFO causes slight chorusing at short reverb amounts, reduce `kLfoDepth` from 12→8 in `tickReverb()`
- The `attenArcSweep` in HTML uses 174 (corrected from 198 in old code, now matches 270° of r=37 arc more closely)

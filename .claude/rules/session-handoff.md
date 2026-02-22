# Session Handoff — BeadyEye
**Last updated:** 2026-02-22 (session 4 — reverb root-cause fixed)
**Branch:** `feat/next-iteration`
**Machine:** macOS (Apple Silicon — Ninja build, Xcode 26.2 stable)

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
| Window size | 800×550 |

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
| `feedback` | float | 0–1 | 0.0 | Effective max = 0.75 (scaled in processBlock) |
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

### Reverb (Dattorro Plate) — CURRENT CORRECT STATE
- **APF formula:** `out = -g·in + (1-g²)·delayed` with `buf = in + g·delayed`
  - Transfer function: H(z) = (−g + z^{−D}) / (1 − g·z^{−D}) — TRUE unity-gain allpass |H|=1
  - Was `out = -g·in + delayed` in all reconstructed versions → DC gain ~2.6× at g=0.70 → blowup
- **Input:** `x = (inL + inR) * 0.5f` — no pre-scale (pre-scale was compensating for wrong APF gain)
- **Decay:** `0.5 + amount * 0.28` → range [0.50, 0.78]
- **Damping (LP in tank feedback):** near-zero at low amounts, steeper above 0.75:
  - `damp = 0.0005 + amount * 0.03` for amount ≤ 0.75
  - `damp = 0.0005 + 0.0225 + (amount - 0.75) * 0.8` above 0.75
- **Output:** `tanh(wetL * 0.4f)` — soft clip; 8-tap sum physically cannot exceed 0 dBFS
- **Bypass:** none — reverb always runs (`tickReverb()` called every sample). At amount=0 the crossfade `inL + (wet-inL)*0 = inL` gives silent wet path but keeps tank warm, preventing burst on re-engage.
- **NaN guard:** resets all APF/DL buffers and feedback state if any tank value is non-finite

### Reverb Architecture (8 APFs, 4 delay lines)
```
apf[0..3]  — input diffusion (4 allpasses in series, g = 0.75 / 0.625)
apf[4..5]  — L tank (entry APF g=0.70, exit APF g=0.50)
apf[6..7]  — R tank (entry APF g=0.70, exit APF g=0.50)
dl[0..1]   — L tank delay lines (full delay + 8 tap reads)
dl[2..3]   — R tank delay lines (full delay + 8 tap reads)
rvFdL/R    — cross-coupled feedback (L feeds R input, R feeds L input)
rvLP1/2    — one-pole LP state (HF damping in feedback path)
```

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
`build-and-install-mac.sh` picks the CMake generator automatically:

| Priority | Generator | When used |
|----------|-----------|-----------|
| 1st | **Ninja** | `ninja` binary found in PATH |
| 2nd | **Xcode** | `xcodebuild -version` exits 0 |
| 3rd | **Unix Makefiles** | fallback |

**Architecture:** defaults to `uname -m` (native). Override for universal binary:
```bash
ARCHS="arm64;x86_64" bash scripts/build-and-install-mac.sh BeadyEye
```

**Cache invalidation:** full build dir wipe if generator changed (juceaide sub-build also caches generator).

---

## Current Status

### Fixed (all sessions)
- [x] **Grain engine** — feedback additive model, size cubic curve, tanh normalization, density sync fallback
- [x] **macOS infra** — `setup-mac.sh`, `system-check-mac.sh`, `pluginval-mac.sh`, `au-cache-bust-mac.sh`
- [x] **Build script** — multi-machine Ninja/Xcode/Unix-Makefiles auto-detection, generator cache invalidation
- [x] **UI overhaul** (session 3): 800px window, TIME/SIZE/SHAPE left cluster, DENSITY hero at 440px, PITCH at 660px, grain viz arc, Shift+click atten mode, output section moved up
- [x] **Reverb APF root-cause fix** (session 4):
  - All prior reconstructed code had `out = -g·in + delayed` → DC gain 2.63× → required compensation hacks
  - Fixed to `out = -g·in + (1-g²)·delayed` → true unity-gain allpass
  - Removed all compensation (pre-scale, aggressive tanh, etc.) — no longer needed
  - Reverb confirmed stable and usable 0–100%
- [x] **Reverb level/boominess** (session 4):
  - Decay capped at 0.78 (`0.5 + amount*0.28`) — was reaching 0.90 → extreme low-mid accumulation past 60%
  - Output soft-clip `tanh(wet * 0.4f)` — 8-tap sum cannot exceed 0 dBFS

### Pending / Next Session
- [ ] pluginval run to confirm AU validation passes
- [ ] Reverb LP damping tuning — currently near-zero at amounts 0–0.75 (almost no HF damping). May want natural plate warmth: raise to `damp = 0.2 + amount * 0.4` range [0.2, 0.6]
- [ ] Consider reverb LFO modulation on tank APFs (breaks up metallic fixed-resonance ringing) — was described in session 3 handoff but NOT implemented in actual code
- [ ] Universal binary: `ARCHS="arm64;x86_64" bash scripts/build-and-install-mac.sh BeadyEye`
- [ ] Commit tag / version bump for release

---

## Known Issues / Watch Points
- Float comparison warning on `currentSlot != lastSyncSlot` (line ~499) — harmless by design (value comes from `std::floor`)
- `density_sync` at density=0 returns `interval=999999` → silent (correct behavior)
- AU cache must be busted after every install on macOS; build script does this automatically
- Universal binary: use `ARCHS="arm64;x86_64" bash scripts/build-and-install-mac.sh BeadyEye`
- The `attenArcSweep` in HTML uses 174 (corrected from 198 in old code, now matches 270° of r=37 arc more closely)

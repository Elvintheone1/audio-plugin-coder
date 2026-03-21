# Splice — DSP Architecture Specification
**Version:** v0.1 | **Date:** 2026-03-20 | **Phase:** Planning

---

## Core Components

### 1. ReelBuffer
Owns the loaded audio data. Responsible for file I/O and sample-accurate access.
- `juce::AudioFormatReader` — decode WAV/AIFF/etc.
- Stores decoded PCM as `juce::AudioBuffer<float>` (stereo, normalized)
- Provides `readSample(channel, position)` with interpolation (linear/hermite)
- Tracks reel metadata: sample rate, length in samples, BPM, beat-to-sample mapping

### 2. ReelSlicer
Maps the grid hierarchy to sample positions within the ReelBuffer.
- Converts `{section, slice}` coordinates → `{startSample, endSample}`
- Knows current grid division (1–16 per section)
- Provides `getSliceRange(sectionIdx, sliceIdx)` → `{start, end}` in samples
- Maintains `slice_active[64]` boolean array (on/off state)
- Recomputes slice boundaries when grid setting changes

### 3. SpliceVoice
One polyphonic voice. Reads the reel buffer at a given slice position, applies envelope and filter.
```
ReelBuffer → [Playhead] → [TimeStretcher] → [AmpEnvelope] → [Filter+FilterEnv] → output
```
Per-voice state:
- `playheadPos` — current read position in samples (float, sub-sample precision)
- `playRate` — samples-per-output-sample (accounts for pitch + time-stretch)
- `direction` — fwd / rev
- `targetSlice` — which slice this voice is playing
- `ampEnv` — AmpEnvelope instance
- `filterEnv` — FilterEnvelope instance
- `filter` — SVF filter instance
- `velocity` — 0.0–1.0
- `pitchOffsetSemitones` — from SEQ PITCH lane or keyboard position

### 4. VoiceManager
Polyphonic voice pool. Manages voice stealing, allocation, and summing.
- Fixed pool of N voices (e.g. 8–16)
- `noteOn(midiNote, velocity, sliceIdx)` → allocates a voice
- `noteOff(midiNote)` → releases voice (enters release phase)
- Sums all active voices to stereo output buffer
- Voice stealing: steal quietest/oldest voice when pool exhausted

### 5. TimeStretcher (Phase 3)
Pitch-independent playback speed control. Allows a slice to play at a different pitch while maintaining its original duration in tempo-relative terms.

**Algorithm: WSOLA (Waveform Similarity Overlap-Add)**
- No FFT required — time-domain algorithm, lower latency than phase vocoder
- Suitable for musical/percussive material
- Pitch shift via resampling + duration correction via OLA
- Per-voice instance (each voice has its own stretcher state)

**Phase 1 placeholder:** simple resampling (`playRate = originalRate × pitchRatio`). Tempo sync will be approximate until Phase 3.

### 6. AmpEnvelope
Standard ADSR envelope. One instance per voice. Re-triggers on each slice event.
- States: IDLE → ATTACK → DECAY → SUSTAIN → RELEASE → IDLE
- Parameters read from global `amp_attack/decay/sustain/release`
- Linear or exponential curves (exponential sounds more natural)
- Thread-safe parameter reads via `std::atomic`

### 7. FilterEnvelope + SVF Filter
Per-voice. Re-triggers on each slice event.

**Filter:** State Variable Filter (SVF) — simultaneous LP/BP/HP outputs, stable at high resonance.
```
// Topology Preserving Transform SVF (Andy Simper / Cytomic)
v1 = a1 * ic1eq + a2 * ic2eq
v2 = a2 * ic1eq + a3 * ic2eq
ic1eq = 2 * v1 - ic1eq
ic2eq = 2 * v2 - ic2eq
```
- Parameters: `filter_cutoff`, `filter_res`, `filter_type`
- Cutoff smoothed per-sample to prevent zipper noise

**Filter Envelope:** AD envelope (no sustain — filter env decays to resting cutoff).
- `fenv_depth` modulates cutoff: `finalCutoff = filter_cutoff × (1 + fenv_depth × envValue)`
- Bipolar depth: positive = opens filter on attack, negative = closes

### 8. CartesianSequencer
SEQ mode engine. Advances 4 independent lanes on the DAW clock, outputs per-step values.

```
DAW PPQ clock
    ↓
CartesianSequencer
├── Lane[POS]   → sliceIndex (0–63) → VoiceManager.noteOn(sliceIdx)
├── Lane[VEL]   → velocity (0–127)  → VoiceManager.noteOn(velocity)
├── Lane[PITCH] → semitones         → SpliceVoice.pitchOffset
└── Lane[DIR]   → fwd/rev           → SpliceVoice.direction
```

### 9. SequencerLane
One 2D grid lane within the CartesianSequencer.
- `float values[16]` — step values (normalized 0–1, interpreted per lane type)
- `int stepCount` — active steps (1–16), determines grid shape
- `TraversalPattern pattern` — forward/reverse/snake/diagonal/random/drunk
- `int currentStep` — current position in traversal sequence
- `TraversalCursor cursor` — {x, y} position in 2D grid
- `advance()` → moves cursor according to pattern, returns next cell index
- Serialized to/from `juce::ValueTree` for DAW state

**Traversal pattern implementations:**
- **Forward:** `x++; if x >= cols: x=0, y++; if y >= rows: y=0`
- **Reverse:** reverse of forward
- **Snake:** even rows L→R, odd rows R→L
- **Diagonal:** step along diagonal, wrap at edges
- **Random:** `{x,y} = uniform random`
- **Drunk:** `{x,y} += random {-1,0,+1} each axis, clamped`

### 10. ModMatrix
One modulation slot per global parameter. Source = LFO OR MIDI CC (mutually exclusive).

```
ModSlot {
    ModSource source;    // LFO | CC | NONE
    float depth;         // 0–1
    int ccNumber;        // if source == CC
    LFOState lfo;        // if source == LFO
}
```

**LFO:**
- Waveshapes: sine, tri, square, saw, S&H (random step)
- Rate: free (Hz) or tempo-synced (1/4 to 1/32)
- Phase accumulator — advances each sample block
- Output range: -1.0 to +1.0, scaled by depth

**MIDI CC:**
- Listens for CC messages in `processBlock`
- Smoothed via one-pole filter to prevent zipper noise
- `currentValue = smooth(ccValue / 127.0)`

### 11. ArpEngine
Arpeggiator for ARP mode.
- Maintains sorted list of held MIDI notes
- `Hold` mode: freezes the note list when engaged
- Advances through notes at `arp_rate` (tempo-synced)
- Patterns: up, down, up-down, as-played, random
- On each arp step: triggers VoiceManager with current note + computed sliceIdx

---

## Processing Chain

### POLY / SLICE / ARP modes:
```
MIDI Input
    ↓
[ArpEngine] (ARP mode only)
    ↓
VoiceManager.noteOn(note, vel, sliceIdx)
    ↓
SpliceVoice ×N (parallel)
    ├── ReelBuffer.read(sliceRange, playheadPos)
    ├── [TimeStretcher] (Phase 3)
    ├── AmpEnvelope.process()
    └── SVFFilter + FilterEnvelope.process()
    ↓
Sum voices → stereo buffer
    ↓
ModMatrix.apply() → parameter modulation
    ↓
Output gain → DAW
```

### SEQ mode:
```
DAW PPQ clock
    ↓
CartesianSequencer.advance()
├── Lane[POS]   → sliceIdx
├── Lane[VEL]   → velocity
├── Lane[PITCH] → pitchOffset
└── Lane[DIR]   → direction
    ↓
VoiceManager.noteOn(sliceIdx, vel, pitch, dir)
    ↓
[same voice chain as above]
```

---

## Parameter Mapping

| Parameter | Component | Notes |
|:----------|:----------|:------|
| `playback_mode` | PluginProcessor | Routes to voice/arp/seq engine |
| `grid` | ReelSlicer | Recomputes slice boundaries |
| `reel_dir` | SpliceVoice | Global playback direction multiplier |
| `slice_dir` | SpliceVoice | Per-slice audio content direction |
| `amp_attack/decay/sustain/release` | AmpEnvelope | Smoothed JUCE params |
| `filter_cutoff/res/type` | SVFFilter | Per-voice, smoothed |
| `fenv_depth/attack/decay` | FilterEnvelope | Per-voice |
| `arp_rate/hold/pattern` | ArpEngine | Mode-gated |
| `lfo_*/cc_*` | ModMatrix | Per-slot |
| `seq_step_length` | CartesianSequencer | Clock division |
| `lane_N_steps/pattern` | SequencerLane | Per-lane |
| `output_vol` | PluginProcessor | Final output gain |

---

## State Serialization

DAW preset save/load via `getStateInformation` / `setStateInformation`:

```xml
<SpliceState>
  <Grid sliceActive="1111101110111101..."/> <!-- 64 bits as string -->
  <SeqLane id="pos" steps="16" pattern="snake" values="0.1,0.5,0.3,..."/>
  <SeqLane id="vel" steps="12" pattern="drunk" values="..."/>
  <SeqLane id="pitch" steps="9" pattern="diagonal" values="..."/>
  <SeqLane id="dir" steps="16" pattern="forward" values="..."/>
</SpliceState>
```

JUCE parameters saved automatically via `AudioProcessorValueTreeState`.

---

## JUCE Modules Required

```cmake
target_link_libraries(Splice PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats     # File loading
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp               # SVF filter helpers
    juce::juce_gui_extra         # WebBrowserComponent
)
```

---

## Complexity Assessment

| Component | Score | Rationale |
|:----------|:------|:----------|
| ReelBuffer + file loading | 2 | Standard JUCE AudioFormatReader |
| ReelSlicer | 2 | Index arithmetic, straightforward |
| VoiceManager + SpliceVoice | 3 | Polyphony, voice stealing, state |
| TimeStretcher (WSOLA) | 5 | Complex algorithm, per-voice state |
| AmpEnvelope | 2 | Standard ADSR |
| SVF Filter + FilterEnvelope | 3 | Cytomic SVF, per-voice |
| CartesianSequencer (4 lanes) | 4 | 2D traversal, polyrhythm, sync |
| ModMatrix (LFO + CC) | 3 | Per-parameter state, smoothing |
| ArpEngine | 3 | Clock sync, hold mode, patterns |
| WebView UI | 4 | Real-time grid, animation, JS↔C++ |
| State serialization | 2 | ValueTree XML |

**Overall Complexity Score: 5 / 5**

The WSOLA time-stretcher and cartesian sequencer are genuinely research-grade components for a first implementation. Recommend deferring WSOLA to Phase 3 with simple resampling as a placeholder.

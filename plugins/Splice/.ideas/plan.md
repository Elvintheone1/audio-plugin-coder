# Splice — Implementation Plan
**Version:** v0.1 | **Date:** 2026-03-20 | **Phase:** Planning

---

## Complexity Score: 5 / 5

High complexity. Phased implementation is mandatory. Do not attempt to build all components in one pass.

---

## UI Framework: WebView

**Rationale:**
- Real-time grid animation (illuminated slices, playhead) requires canvas rendering
- Cycles-inspired visual language suits HTML5 Canvas + CSS
- Interactive 2D grid (click-to-toggle, SEQ lane grids) suits JS event handling
- Pattern proven with BeadyEye in this repo

---

## Phased Implementation Strategy

### Phase 1 — Foundation (Buildable, Testable Shell)
**Goal:** Plugin loads, file can be loaded, single-voice SLICE mode works. No time-stretch.

- [ ] CMakeLists.txt — project setup, JUCE modules, WebView flag
- [ ] PluginProcessor skeleton — APVTS, parameter registration
- [ ] ReelBuffer — file loading via `juce::AudioFormatReader`, stereo PCM storage
- [ ] ReelSlicer — slice boundary computation, `slice_active[64]` state
- [ ] SpliceVoice (basic) — single voice, simple resampling, fwd/rev direction
- [ ] VoiceManager (basic) — monophonic to start, expand in Phase 2
- [ ] SLICE mode — C1-upward keyboard map, trigger individual slices
- [ ] AmpEnvelope — ADSR, re-triggers per slice event
- [ ] PluginEditor skeleton — WebView wired up, loads `index.html`
- [ ] index.html v1 — 4 section blocks, slices as colored squares, click to toggle, playhead highlight

**Exit criteria:** Can load a WAV, play individual slices via MIDI keyboard, slices illuminate on playback, click to mute/unmute.

---

### Phase 2 — Polyphony + Filter
**Goal:** Full polyphonic playback. Filter and filter envelope working.

- [ ] VoiceManager — full polyphonic pool (8 voices), voice stealing
- [ ] POLY mode — chromatic MIDI map, reel plays at transposed pitch (simple resampling)
- [ ] SVF Filter — Cytomic topology, LP/BP/HP, per-voice
- [ ] FilterEnvelope — AD envelope, bipolar depth, re-triggers per slice
- [ ] Reel direction + Slice direction — fwd/rev/pingpong/random
- [ ] `reel_dir` / `slice_dir` parameters wired to voice engine
- [ ] Output gain

**Exit criteria:** Hold chord → polyphonic reel playback at different pitches. Filter sweeps audible.

---

### Phase 3 — Time Stretch + ARP
**Goal:** True pitch-independent time stretching. ARP mode functional.

- [ ] WSOLA TimeStretcher — per-voice, pitch-independent playback
  - Analysis window + synthesis window
  - Waveform similarity search (autocorrelation)
  - Overlap-add output
- [ ] Integrate TimeStretcher into SpliceVoice
- [ ] POLY mode upgraded — now truly time-stretched
- [ ] ArpEngine — held note list, Hold mode, 5 patterns (up/down/updown/as-played/random)
- [ ] ARP mode — tempo-synced rate (1/4, 1/8, 1/16, 1/32)

**Exit criteria:** Play C4 vs C5 — same rhythm, different pitch. ARP holds and arpegiates correctly.

---

### Phase 4 — Cartesian Sequencer
**Goal:** SEQ mode fully functional with all 4 lanes and traversal patterns.

- [ ] SequencerLane — 2D grid state, step count, traversal cursor
- [ ] Traversal patterns — forward, reverse, snake, diagonal, random, drunk
- [ ] CartesianSequencer — 4 lanes (POS/VEL/PITCH/DIR), shared clock
- [ ] Polyrhythm — each lane has independent step count
- [ ] SEQ mode wiring — POS → slice trigger, VEL → velocity, PITCH → offset, DIR → direction
- [ ] State serialization — lane grids to/from ValueTree XML
- [ ] SEQ page UI — 4 lane blocks, 2D grid cells, playhead per lane, pattern selector

**Exit criteria:** SEQ mode drives playback. Different step counts per lane create polyrhythm. Pattern changes alter traversal visibly and audibly.

---

### Phase 5 — Mod Matrix + Polish
**Goal:** Complete modulation system. DAW integration. Preset save/load.

- [ ] ModMatrix — one slot per parameter, LFO or CC source
- [ ] LFO — 5 waveshapes, free/synced rate, depth
- [ ] MIDI CC — assignable CC number, smooth interpolation
- [ ] DAW clock sync — PPQ position for all tempo-synced components
- [ ] Drag & drop reel loading
- [ ] Full preset save/load (APVTS + custom ValueTree state)
- [ ] UI polish — mod assignment UI, visual feedback for active mod
- [ ] pluginval pass

**Exit criteria:** Mod wheel modulates a parameter. LFO on filter cutoff audible. Presets save and reload correctly. pluginval passes.

---

## Dependencies

**JUCE Modules:**
```cmake
juce_audio_basics
juce_audio_formats      # WAV/AIFF loading
juce_audio_processors
juce_audio_utils
juce_dsp
juce_gui_extra          # WebBrowserComponent
```

**External:** None (WSOLA implemented from scratch — avoids RubberBand licensing/linking complexity)

---

## Risk Assessment

**High Risk:**
- **WSOLA time-stretcher** — complex algorithm, artifacts at extremes, per-voice state management. Defer to Phase 3. Use simple resampling as placeholder.
- **Cartesian traversal + polyrhythm** — clock arithmetic, off-by-one errors, musical correctness. Test each pattern in isolation.
- **WebView ↔ C++ real-time communication** — grid state updates must not allocate on audio thread. Use lock-free message queue or atomic flags.

**Medium Risk:**
- **Voice stealing** — wrong stealing strategy creates audible artifacts. Use quietest+oldest heuristic.
- **Filter stability** — SVF at high resonance + high cutoff rate of change can blow up. Smooth parameters per-sample.
- **DAW clock sync** — PPQ drift between hosts. Test in Logic, Ableton, and standalone.

**Low Risk:**
- **File loading** — standard JUCE AudioFormatReader, well-understood
- **AmpEnvelope** — standard ADSR, low risk
- **ARP engine** — straightforward list traversal

---

## File Structure

```
plugins/Splice/
├── CMakeLists.txt
├── status.json
├── .ideas/
│   ├── creative-brief.md
│   ├── parameter-spec.md
│   ├── architecture.md
│   └── plan.md
└── Source/
    ├── PluginProcessor.h / .cpp
    ├── PluginEditor.h / .cpp
    ├── ReelBuffer.h / .cpp
    ├── ReelSlicer.h / .cpp
    ├── SpliceVoice.h / .cpp
    ├── VoiceManager.h / .cpp
    ├── AmpEnvelope.h
    ├── SVFFilter.h
    ├── FilterEnvelope.h
    ├── TimeStretcher.h / .cpp      (Phase 3)
    ├── CartesianSequencer.h / .cpp (Phase 4)
    ├── SequencerLane.h / .cpp      (Phase 4)
    ├── ModMatrix.h / .cpp          (Phase 5)
    ├── ArpEngine.h / .cpp          (Phase 3)
    └── ui/
        └── public/
            └── index.html
```

---

## CMakeLists.txt Key Settings

```cmake
juce_add_plugin(Splice
    PLUGIN_CODE         Splc
    PLUGIN_MANUFACTURER_CODE AJzz
    PRODUCT_NAME        "Splice"
    COMPANY_NAME        "Noizefield"
    FORMATS             VST3 AU Standalone
    IS_SYNTH            TRUE
    NEEDS_MIDI_INPUT    TRUE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    NEEDS_WEB_BROWSER   TRUE          # macOS/Linux WebView
    NEEDS_WEBVIEW2      FALSE         # Windows (set TRUE for Windows builds)
)
```

---

## Session Handoff Notes

- Start with Phase 1. Do not begin Phase 2 until Phase 1 exit criteria are met.
- WSOLA is the hardest single component. Research Andy Simper's WSOLA paper before Phase 3.
- The WebView grid illumination must be driven by a lock-free atomic from the audio thread — never post directly to JS from `processBlock`.
- `slice_active[64]` is serialized state, not a JUCE parameter. Handle separately in `getStateInformation`.
